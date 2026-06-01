#include "vault_controller.h"
#include "serial_bridge.h"
#include <QDebug>
#include <QTimer>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QtConcurrent/QtConcurrent>
#include <memory>
#include <atomic>
#include <functional>
#include <QSet>
#include <QStorageInfo>
#include <QSettings>
#include <QEventLoop>
#include <QSet>

/* Unix socket + HAL iletişimi için */
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/wait.h>

VaultController *g_vaultController = nullptr;
std::atomic<bool> VaultController::s_emergencyClosing{false};

static const char kMapperPath[] = "/dev/mapper/digitalvault";

namespace {

constexpr const char kDefaultUartHost[] = "/tmp/biometric_vault_tty_host";
constexpr const char kDefaultUartDevice[] = "/tmp/biometric_vault_tty";

} // namespace

/*
 * HAL ile doğrudan socket üzerinden iletişim kurar.
 * Bu sayede GUI, C kütüphanesine link etmeden simülatörle konuşabilir.
 */
static QByteArray sendSocketCommand(const QString &socketPath, const QJsonObject &cmd)
{
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return QByteArray();

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath.toUtf8().constData(), sizeof(addr.sun_path) - 1);

    if (::connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ::close(sock);
        return QByteArray();
    }

    QJsonDocument doc(cmd);
    QByteArray data = doc.toJson(QJsonDocument::Compact) + "\n";
    ssize_t bytesWritten = ::write(sock, data.constData(), data.size());
    Q_UNUSED(bytesWritten);

    /* Yanıt oku */
    QByteArray response;
    char buf[4096];
    ssize_t n;
    while ((n = ::read(sock, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        response.append(buf, n);
        if (response.contains('\n')) break;
    }

    ::close(sock);
    return response;
}

VaultController::VaultController(QObject *parent)
    : QObject(parent)
    , m_isLocked(true)
    , m_autoLockTimeout(5)
    , m_isSetupRunning(false)
    , m_isToggling(false)
    , m_setupStatusText("")
    /* Default mount dir; safe lock uses findmnt/mountpoint probes, not path alone. */
    , m_mountPoint(QDir::homePath() + "/VaultMount")
    , m_lastActivity("—")
    , m_socketPath("/tmp/biometric_vault.sock")
    , m_simProcess(nullptr)
    , m_pollTimer(nullptr)
    , m_setupWatcher(nullptr)
    , m_selectedDevice("")
    , m_isSelectedDeviceVault(false)
    , m_selectedDeviceCapacityMB(0)
{
    /* Proje dizinini bul (DigitalVault klasörünü bulana kadar yukarı çık) */
    m_projectDir = QCoreApplication::applicationDirPath();
    QDir d(m_projectDir);
    bool found = false;
    // Maksimum 5 seviye yukarı çıkıp DigitalVault klasörünü ara
    for (int i = 0; i < 5; ++i) {
        if (d.dirName() == "DigitalVault" || d.exists("gui/Dashboard.qml")) {
            found = true;
            break;
        }
        if (!d.cdUp()) break;
    }
    m_projectDir = d.absolutePath();
    if (!found) {
        // Fallback: Eğer bulunamazsa binary'nin olduğu yer
        m_projectDir = QCoreApplication::applicationDirPath();
    }

    /* vault_manager binary'sini bul */
    m_vaultManagerBin = findVaultManager();

    const QString dataRoot = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                             + "/DigitalVault";
    m_registryPath = dataRoot + "/vault_registry.json";
    m_containersDir = dataRoot + "/containers";
    m_statePath = dataRoot + "/vault_state.json";
    QDir().mkpath(m_containersDir);
    QDir().mkpath(QFileInfo(m_registryPath).absolutePath());

    m_vaultManagerBinCache = m_vaultManagerBin.toUtf8();
    m_mountPointCache = m_mountPoint.toUtf8();
    g_vaultController = this;

    m_serialBridge = new SerialBridge(this);
    loadSensorSlotCache();
    connect(m_serialBridge, &SerialBridge::listEnrolledCompleted,
            this, &VaultController::onListEnrolledCompleted);
    connect(m_serialBridge, &SerialBridge::verifyCompleted, this,
            [this](bool ok, int matchedId, const QString &err) {
                handleVerifyCompleted(ok, matchedId, err);
            });
    connect(m_serialBridge, &SerialBridge::sensorEvent, this,
            [this](int code) { handleSensorEvent(code); });
    connect(m_serialBridge, &SerialBridge::operationRejected, this,
            [this](const QString &reason) {
                if (!m_fpUnlockActive && !isLiveFingerprintFlow(m_fpFlow))
                    return;
                abortFingerprintOperationUi();
                emit notificationReceived(
                    reason.isEmpty()
                        ? QStringLiteral("Sensor is busy — please wait")
                        : reason,
                    true);
            });
    connect(m_serialBridge, &SerialBridge::enrollCompleted,
            this, [this](bool ok, int fingerId, const QString &err) {
        if (m_fpFlow == FpFlow::GrantEnroll) {
            const QString device = m_fpContextDevice;
            const QString label = m_pendingGrantLabel;
            if (ok && fingerId >= 1) {
                setFingerprintPromptOutcome(1,
                                            QStringLiteral("Enrollment complete"));
                QTimer::singleShot(600, this, [this, device, label, fingerId]() {
                    if (!appendAccessListEntry(device, fingerId, label, false)) {
                        m_pendingRegistryRollbackSlot = true;
                        if (m_serialBridge && m_serialBridge->isConnected())
                            m_serialBridge->deleteFinger(fingerId);
                        else
                            m_pendingRegistryRollbackSlot = false;
                    } else {
                        const QString displayName =
                            label.isEmpty() ? QStringLiteral("User") : label;
                        emit notificationReceived(
                            QStringLiteral("Access granted: %1.").arg(displayName),
                            false);
                        emit statusChanged();
                        emit devicesChanged();
                        emit vaultAccessListChanged();
                    }
                    m_pendingGrantOwnerId = -1;
                    m_pendingGrantFingerId = -1;
                    m_pendingGrantLabel.clear();
                    m_fpContextDevice.clear();
                    endFingerprintOperationUi();
                });
            } else {
                const QString failMsg =
                    err.isEmpty() ? QStringLiteral("Fingerprint enrollment failed") : err;
                m_fpFlow = FpFlow::None;
                clearFingerprintFlowState();
                m_pendingGrantOwnerId = -1;
                m_pendingGrantFingerId = -1;
                m_pendingGrantLabel.clear();
                m_fpContextDevice.clear();
                setFingerprintPromptOutcome(2, failMsg);
                emit notificationReceived(failMsg, true);
            }
            return;
        }

        if (m_fpFlow != FpFlow::SetupEnroll)
            return;

        if (ok && fingerId >= 1) {
            setFingerprintPromptOutcome(1, QStringLiteral("Enrollment complete"));
            QTimer::singleShot(600, this, [this, fingerId]() {
                m_setupOwnerFingerId = fingerId;
                m_setupOwnerEnrolledThisSession = true;
                m_setupEnrollKeyGraceUntilMs =
                    QDateTime::currentMSecsSinceEpoch() + 10 * 60 * 1000;
                m_pendingEnrollRetryAfterExists = false;
                markSensorSlot(fingerId, true);
                emit setupOwnerFingerIdChanged();
                emit notificationReceived(
                    QStringLiteral("Owner fingerprint enrolled successfully."), false);
                endFingerprintOperationUi();
            });
        } else {
            m_pendingEnrollRetryAfterExists = false;
            m_setupOwnerFingerId = -1;
            emit setupOwnerFingerIdChanged();
            const QString failMsg =
                err.isEmpty() ? QStringLiteral("Fingerprint enrollment failed") : err;
            m_fpFlow = FpFlow::None;
            clearFingerprintFlowState();
            setFingerprintPromptOutcome(2, failMsg);
            emit notificationReceived(failMsg, true);
        }
    });
    connect(m_serialBridge, &SerialBridge::deleteCompleted,
            this, [this](bool ok, int fingerId, const QString &err) {
        if (m_pendingRegistryRollbackSlot) {
            m_pendingRegistryRollbackSlot = false;
            if (!ok) {
                emit notificationReceived(
                    err.isEmpty()
                        ? QStringLiteral(
                              "Kayıt defteri güncellenemedi; sensör slotu temizlenemedi (slot %1)")
                              .arg(fingerId)
                        : err,
                    true);
            } else {
                markSensorSlot(fingerId, false);
                emit notificationReceived(
                    QStringLiteral("Could not save access list; sensor slot %1 was cleared.")
                        .arg(fingerId),
                    true);
            }
            return;
        }
        if (m_pendingSetupVerifyRollback >= 0) {
            const int slot = m_pendingSetupVerifyRollback;
            m_pendingSetupVerifyRollback = -1;
            if (ok)
                markSensorSlot(slot, false);
            Q_UNUSED(err);
            Q_UNUSED(fingerId);
            return;
        }
        onDeleteCompletedForEnroll(ok, fingerId, err);
    });
    connect(m_serialBridge, &SerialBridge::keyReceived,
            this, [this](const QByteArray &key) {
        if (key.size() != 32)
            return;
        if (m_fpFlow == FpFlow::SetupVerify) {
            m_pendingSetupGetKeyOnly = false;
            clearSetupWizardSession();
            m_fpUnlockActive = false;
            m_fpFlow = FpFlow::None;
            emit fingerprintUnlockActiveChanged();
            finishSetupWithKey(key);
            return;
        }
        if (m_fpFlow != FpFlow::Unlock || !m_fpUnlockActive)
            return;
        m_fpUnlockActive = false;
        m_fpFlow = FpFlow::None;
        emit fingerprintUnlockActiveChanged();
        finishUnlockWithKey(key);
    });
    connect(m_serialBridge, &SerialBridge::connectionChanged,
            this, &VaultController::onSerialConnectionChanged);
    connect(m_serialBridge, &SerialBridge::connectionLost,
            this, &VaultController::onSerialConnectionLost);

    m_inventoryRefreshTimer = new QTimer(this);
    m_inventoryRefreshTimer->setSingleShot(true);
    m_inventoryRefreshTimer->setInterval(1200);
    connect(m_inventoryRefreshTimer, &QTimer::timeout, this,
            &VaultController::performSensorInventoryRefresh);

    m_serialBusyWatchdog = new QTimer(this);
    m_serialBusyWatchdog->setSingleShot(true);
    m_serialBusyWatchdog->setInterval(20000);
    connect(m_serialBusyWatchdog, &QTimer::timeout, this, &VaultController::onSerialBusyWatchdog);
    connect(m_serialBridge, &SerialBridge::busyChanged, this, [this]() {
        if (!m_serialBridge || !m_serialBusyWatchdog)
            return;
        if (m_serialBridge->isBusy())
            m_serialBusyWatchdog->start();
        else
            m_serialBusyWatchdog->stop();
    });

    updateSensorStatus(SensorStatus::Unconfigured);
    scheduleSerialConnectRetries();

    syncRegistryWithLosetup();
    refreshStorageDevices();

    /* Mount dizinini oluştur */
    QDir().mkpath(m_mountPoint);

    /* Başlangıçta simülatör durumunu kontrol et ve parmak izlerini yükle */
    QTimer::singleShot(500, this, [this]() {
        loadFingerprintsFromHAL();
    });

    /* Periyodik durum kontrolü */
    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &VaultController::pollStatus);
    m_pollTimer->start(2000);

    m_logFilePath = m_projectDir + "/security_logs.json";
    
    /* Auto-lock timer ayarla */
    m_autoLockTimer = new QTimer(this);
    m_autoLockTimer->setSingleShot(true);
    connect(m_autoLockTimer, &QTimer::timeout, this, &VaultController::handleAutoLock);
    resetAutoLockTimer();

    // Eğer log dosyası yoksa boş bir array ile oluştur
    if (!QFile::exists(m_logFilePath)) {
        QFile file(m_logFilePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write("[]");
            file.close();
        }
    } else {
        // Başlangıçta son logları yükle
        m_securityLogs = getSecurityLogsPage(0, 50);
        emit logsChanged();
    }

    QTimer::singleShot(1000, this, &VaultController::startupRecovery);
}

const char *VaultController::cachedVaultManagerBin() const
{
    return m_vaultManagerBinCache.constData();
}

const char *VaultController::cachedMountPoint() const
{
    return m_mountPointCache.constData();
}

int VaultController::posixEmergencyClose(const char *vaultManagerBin, const char *mountPoint)
{
    if (!vaultManagerBin || !mountPoint || !*vaultManagerBin || !*mountPoint)
        return -1;

    const pid_t pid = fork();
    if (pid < 0)
        return -1;
    if (pid == 0) {
        execlp("pkexec", "pkexec", vaultManagerBin, "forceclose", mountPoint, nullptr);
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
        return -1;
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return -1;
}

bool VaultController::vaultRuntimeOpen() const
{
    return isVaultRuntimeOpen();
}

bool VaultController::isMapperPresent() const
{
    return QFile::exists(QString::fromUtf8(kMapperPath));
}

bool VaultController::isUserVaultMounted() const
{
    QProcess proc;
    proc.start(QStringLiteral("findmnt"),
               {QStringLiteral("-no"), QStringLiteral("SOURCE"),
                QStringLiteral("--target"), m_mountPoint});
    if (!proc.waitForFinished(1000) || proc.exitCode() != 0)
        return false;

    const QString src = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    return src.contains(QStringLiteral("digitalvault"));
}

bool VaultController::isVaultRuntimeOpen() const
{
    if (isUserVaultMounted())
        return true;
    if (!m_unlockedDevice.isEmpty() && isMapperPresent())
        return true;
    return false;
}

void VaultController::syncRuntimeFromKernel()
{
    const bool userMounted = isUserVaultMounted();
    const bool mapper = isMapperPresent();

    if (userMounted) {
        if (m_isLocked) {
            m_isLocked = false;
            emit statusChanged();
        }
        return;
    }

    if (!mapper && !m_isLocked) {
        m_isLocked = true;
        m_unlockedDevice.clear();
        emit statusChanged();
    } else if (mapper && m_unlockedDevice.isEmpty() && !m_isLocked) {
        m_isLocked = true;
        emit statusChanged();
    }
}

bool VaultController::reconcileVaultRuntime()
{
    const int ret = runVaultManager({QStringLiteral("reconcile"), m_mountPoint}, 25000);
    syncRuntimeFromKernel();
    return ret == 0;
}

bool VaultController::emergencyClose()
{
    if (s_emergencyClosing.exchange(true))
        return m_isLocked;

    bool ok = true;
    syncRuntimeFromKernel();

    if (isUserVaultMounted()) {
        const int ret = runVaultManager({QStringLiteral("forceclose"), m_mountPoint}, 20000);
        ok = (ret == 0);
    } else if (isMapperPresent()) {
        ok = reconcileVaultRuntime();
    }

    m_isLocked = true;
    m_unlockedDevice.clear();
    saveState(QStringLiteral("locked"), false);
    emit statusChanged();

    if (!ok)
        addLog("ERROR", QStringLiteral("Emergency lock / reconcile failed."));

    s_emergencyClosing.store(false);
    return ok;
}

void VaultController::saveState(const QString &lastKnownState, bool setupInProgress)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("lastKnownState"), lastKnownState);
    obj.insert(QStringLiteral("setupInProgress"), setupInProgress);
    obj.insert(QStringLiteral("lastDevice"), m_selectedDevice);
    obj.insert(QStringLiteral("lastUnlockedDevice"), m_unlockedDevice);
    obj.insert(QStringLiteral("lastMountPoint"), m_mountPoint);
    obj.insert(QStringLiteral("timestamp"),
               QDateTime::currentDateTime().toString(Qt::ISODate));

    const QString tmpPath = m_statePath + QStringLiteral(".tmp");
    QFile tmp(tmpPath);
    if (!tmp.open(QIODevice::WriteOnly))
        return;
    tmp.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    tmp.close();
    QFile::remove(m_statePath);
    QFile::rename(tmpPath, m_statePath);
}

QJsonObject VaultController::loadState() const
{
    QFile f(m_statePath);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

void VaultController::startupRecovery()
{
    const QJsonObject state = loadState();

    if (isMapperPresent() || isUserVaultMounted()) {
        addLog("WARNING", "Orphan vault mapper/mount detected at startup — reconciling.");
        reconcileVaultRuntime();
    }

    if (isVaultRuntimeOpen()) {
        addLog("WARNING", "Vault left open at startup — locking.");
        QTimer::singleShot(500, this, [this]() {
            if (emergencyClose())
                emit notificationReceived("Vault was left open; it has been locked.", false);
            else
                emit notificationReceived(
                    "Could not lock orphaned vault. Close apps using the mount point.", true);
        });
    } else if (!m_isLocked) {
        m_isLocked = true;
        saveState(QStringLiteral("locked"), false);
        emit statusChanged();
    }

    if (state.value(QStringLiteral("setupInProgress")).toBool()) {
        QTimer::singleShot(1500, this, [this]() {
            saveState(QStringLiteral("locked"), false);
            emit notificationReceived(
                "Previous setup was interrupted. Verify the device before retrying.", true);
        });
    }
}

bool VaultController::sensorConnected() const
{
    if (m_serialBridge && m_serialBridge->isConnected())
        return true;
    return m_sensorStatus == QStringLiteral("connected");
}

void VaultController::onSerialBusyWatchdog()
{
    if (!m_serialBridge || !m_serialBridge->isBusy())
        return;
    ++m_serialExclusiveGeneration;
    m_serialBridge->forceIdle();
    if (m_fpUnlockActive) {
        m_fpUnlockActive = false;
        m_fpUnlockMessage.clear();
        clearFingerprintFlowState();
        emit fingerprintUnlockActiveChanged();
    }
    emit notificationReceived(
        QStringLiteral("Sensor operation timed out — UI unlocked"), true);
}

bool VaultController::simulatorRunning() const
{
    if (m_serialBridge && m_serialBridge->isConnected())
        return true;
    if (m_simProcess && m_simProcess->state() == QProcess::Running)
        return true;
    return QFile::exists(m_socketPath);
}

QString VaultController::resolveSerialPort()
{
    QSettings settings;
    QString port = settings.value(QStringLiteral("hal/serialPort")).toString().trimmed();
    if (!port.isEmpty() && !QFile::exists(port)) {
        settings.remove(QStringLiteral("hal/serialPort"));
        port.clear();
    }
    if (port.isEmpty() && QFile::exists(QString::fromUtf8(kDefaultUartHost)))
        port = QString::fromUtf8(kDefaultUartHost);
    if (port.isEmpty()
        && settings.value(QStringLiteral("hal/autoDetectUsb"), false).toBool()) {
        const QDir devDir(QStringLiteral("/dev"));
        const auto filters = QDir::System | QDir::Files | QDir::Readable;
        const QStringList names =
            devDir.entryList({QStringLiteral("ttyUSB*"), QStringLiteral("ttyACM*")},
                             filters, QDir::Name);
        if (!names.isEmpty())
            port = QStringLiteral("/dev/") + names.first();
    }
    m_sensorPortPathDisplay = port;
    return port;
}

void VaultController::updateSensorStatus(SensorStatus status, const QString &customText)
{
    QString statusKey;
    QString text = customText;
    switch (status) {
    case SensorStatus::Unconfigured:
        statusKey = QStringLiteral("unconfigured");
        if (text.isEmpty())
            text = QStringLiteral("Parmak izi sensörü yapılandırılmadı");
        break;
    case SensorStatus::Disconnected:
        statusKey = QStringLiteral("disconnected");
        if (text.isEmpty())
            text = QStringLiteral("Sensör bağlı değil");
        break;
    case SensorStatus::Connecting:
        statusKey = QStringLiteral("connecting");
        if (text.isEmpty())
            text = QStringLiteral("Bağlanıyor…");
        break;
    case SensorStatus::Connected:
        statusKey = QStringLiteral("connected");
        if (text.isEmpty())
            text = QStringLiteral("Sensör bağlı");
        break;
    case SensorStatus::Error:
        statusKey = QStringLiteral("error");
        if (text.isEmpty())
            text = QStringLiteral("Sensör bağlantı hatası");
        break;
    }
    if (m_sensorStatus == statusKey && m_sensorStatusText == text)
        return;
    m_sensorStatus = statusKey;
    m_sensorStatusText = text;
    emit sensorStatusChanged();
    emit statusChanged();
}

void VaultController::endStartupConnectPhase()
{
    m_startupConnectPhase = false;
}

void VaultController::clearFailedSerialPortSetting()
{
    if (m_lastAttemptedPort.isEmpty())
        return;
    QSettings settings;
    if (settings.value(QStringLiteral("hal/serialPort")).toString() == m_lastAttemptedPort)
        settings.remove(QStringLiteral("hal/serialPort"));
}

void VaultController::onSerialConnectionLost(const QString &reason)
{
    ++m_serialExclusiveGeneration;
    m_serialConnectQueued = false;
    const bool wasConnected = (m_sensorStatus == QStringLiteral("connected"));
    const bool hadFpOp = m_fpUnlockActive;
    const bool grantEnrollOrphan = (m_fpFlow == FpFlow::GrantEnroll);

    if (m_fpUnlockActive) {
        if (grantEnrollOrphan)
            cleanupGrantEnrollOrphan();
        m_fpUnlockActive = false;
        m_fpUnlockMessage.clear();
        clearFingerprintFlowState();
        emit fingerprintUnlockActiveChanged();
        if (hadFpOp) {
            emit notificationReceived(
                QStringLiteral("Fingerprint operation cancelled: sensor disconnected (%1)")
                    .arg(reason.isEmpty() ? QStringLiteral("unknown") : reason),
                true);
        }
    }

    if (m_startupConnectPhase) {
        clearFailedSerialPortSetting();
        const QString port = resolveSerialPort();
        updateSensorStatus(port.isEmpty() ? SensorStatus::Unconfigured : SensorStatus::Disconnected);
        return;
    }

    if (m_manualConnectRequested) {
        m_manualConnectRequested = false;
        clearFailedSerialPortSetting();
        updateSensorStatus(SensorStatus::Error,
                           QStringLiteral("Sensör açılamadı: %1").arg(reason));
        emit notificationReceived(
            QStringLiteral("Sensör açılamadı: %1").arg(reason), true);
        return;
    }

    if (!m_lastAttemptedPort.isEmpty())
        m_sensorPortPathDisplay = m_lastAttemptedPort;
    const QString port = resolveSerialPort();
    updateSensorStatus(port.isEmpty() ? SensorStatus::Unconfigured : SensorStatus::Disconnected);

    if (wasConnected) {
        emit notificationReceived(QStringLiteral("Sensör bağlantısı kesildi"), true);
    }
}

void VaultController::tryConnectSerial()
{
    if (!m_serialBridge)
        return;
    const QString port = resolveSerialPort();
    if (port.isEmpty()) {
        updateSensorStatus(SensorStatus::Unconfigured);
        return;
    }
    if (m_serialBridge->isConnected() && m_serialBridge->portName() == port) {
        m_lastAttemptedPort = port;
        m_sensorPortPathDisplay = port;
        if (m_sensorStatus != QStringLiteral("connected"))
            updateSensorStatus(SensorStatus::Connected);
        return;
    }
    if (m_serialConnectQueued && m_lastAttemptedPort == port)
        return;

    m_lastAttemptedPort = port;
    m_sensorPortPathDisplay = port;
    m_serialConnectQueued = true;
    updateSensorStatus(SensorStatus::Connecting);
    m_serialBridge->connectToPort(port);
}

void VaultController::connectToConfiguredPort(const QString &port)
{
    const QString trimmed = port.trimmed();
    if (trimmed.isEmpty()) {
        emit notificationReceived(QStringLiteral("Seri port seçilmedi"), true);
        return;
    }
    if (!QFile::exists(trimmed)) {
        emit notificationReceived(
            QStringLiteral("Port bulunamadı: %1").arg(trimmed), true);
        updateSensorStatus(SensorStatus::Error,
                           QStringLiteral("Port bulunamadı: %1").arg(trimmed));
        return;
    }
    QSettings settings;
    settings.setValue(QStringLiteral("hal/serialPort"), trimmed);
    m_manualConnectRequested = true;
    m_startupConnectPhase = false;
    ++m_serialRetryGeneration;
    m_lastAttemptedPort = trimmed;
    m_sensorPortPathDisplay = trimmed;
    if (m_serialBridge && m_serialBridge->isConnected()
        && m_serialBridge->portName() == trimmed) {
        updateSensorStatus(SensorStatus::Connected);
        return;
    }
    if (m_serialConnectQueued && m_lastAttemptedPort == trimmed)
        return;
    m_serialConnectQueued = true;
    updateSensorStatus(SensorStatus::Connecting);
    if (m_serialBridge)
        m_serialBridge->connectToPort(trimmed);
}

QStringList VaultController::availableSerialPorts()
{
    QStringList ports;
    const QDir devDir(QStringLiteral("/dev"));
    const auto filters = QDir::System | QDir::Files | QDir::Readable;
    ports << devDir.entryList({QStringLiteral("ttyUSB*"), QStringLiteral("ttyACM*")},
                              filters, QDir::Name);
    for (int i = 0; i < ports.size(); ++i)
        ports[i].prepend(QStringLiteral("/dev/"));
    const QString saved = QSettings().value(QStringLiteral("hal/serialPort")).toString().trimmed();
    if (!saved.isEmpty() && QFile::exists(saved) && !ports.contains(saved))
        ports.append(saved);
    ports.sort(Qt::CaseInsensitive);
    return ports;
}

void VaultController::clearSerialPortSettings()
{
    QSettings().remove(QStringLiteral("hal/serialPort"));
    m_lastAttemptedPort.clear();
    m_sensorPortPathDisplay.clear();
    updateSensorStatus(SensorStatus::Unconfigured);
}

bool VaultController::autoDetectUsb() const
{
    return QSettings().value(QStringLiteral("hal/autoDetectUsb"), false).toBool();
}

void VaultController::setAutoDetectUsb(bool enabled)
{
    QSettings settings;
    if (settings.value(QStringLiteral("hal/autoDetectUsb"), false).toBool() == enabled)
        return;
    settings.setValue(QStringLiteral("hal/autoDetectUsb"), enabled);
    emit autoDetectUsbChanged();
}

QString VaultController::savedSerialPort() const
{
    return QSettings().value(QStringLiteral("hal/serialPort")).toString().trimmed();
}

void VaultController::reconnectSerial()
{
    const QString port = resolveSerialPort();
    if (port.isEmpty()) {
        updateSensorStatus(SensorStatus::Unconfigured);
        return;
    }
    connectToConfiguredPort(port);
}

void VaultController::scheduleSerialConnectRetries()
{
    if (resolveSerialPort().isEmpty()) {
        endStartupConnectPhase();
        return;
    }
    const int gen = ++m_serialRetryGeneration;
    tryConnectSerial();
    QTimer::singleShot(2000, this, [this, gen]() {
        if (gen != m_serialRetryGeneration)
            return;
        if (m_serialBridge && m_serialBridge->isConnected()) {
            endStartupConnectPhase();
            return;
        }
        tryConnectSerial();
        endStartupConnectPhase();
    });
}

void VaultController::onSerialConnectionChanged()
{
    if (!m_serialBridge) {
        emit statusChanged();
        return;
    }

    m_serialConnectQueued = false;

    const bool wasConnected = (m_sensorStatus == QStringLiteral("connected"));

    if (m_serialBridge->isConnected()) {
        endStartupConnectPhase();
        m_manualConnectRequested = false;
        ++m_serialRetryGeneration;
        m_sensorPortPathDisplay = m_serialBridge->portName();
        updateSensorStatus(SensorStatus::Connected);
        if (!wasConnected) {
            emit notificationReceived(QStringLiteral("Fingerprint sensor connected"), false);
        }
    } else {
        const QString port = resolveSerialPort();
        updateSensorStatus(port.isEmpty() ? SensorStatus::Unconfigured : SensorStatus::Disconnected);
    }
    emit statusChanged();
}

void VaultController::beginFingerprintUnlockAfterConnect()
{
    if (!m_serialBridge || !m_serialBridge->isConnected()) {
        emit notificationReceived(QStringLiteral("Fingerprint sensor not connected"), true);
        return;
    }
    beginFingerprintUnlock();
}

void VaultController::beginSetupOwnerEnrollAfterConnect(int fingerId)
{
    if (!m_serialBridge || !m_serialBridge->isConnected()) {
        emit notificationReceived(QStringLiteral("Fingerprint sensor not connected"), true);
        return;
    }
    beginSetupOwnerEnroll(fingerId);
}

QSet<int> VaultController::collectUsedFingerSlots(const QString &excludeDevicePath) const
{
    QSet<int> used;
    const QJsonArray arr = loadRegistryArray();
    for (const QJsonValue &v : arr) {
        const QJsonObject reg = v.toObject();
        if (reg.value(QStringLiteral("path")).toString() == excludeDevicePath)
            continue;
        const int owner = reg.value(QStringLiteral("ownerFingerId")).toInt(-1);
        if (owner >= 1 && owner <= 127)
            used.insert(owner);
        const QJsonArray list = reg.value(QStringLiteral("accessList")).toArray();
        for (const QJsonValue &entry : list) {
            const int fid = entry.toObject().value(QStringLiteral("fingerId")).toInt(-1);
            if (fid >= 1 && fid <= 127)
                used.insert(fid);
        }
    }
    return used;
}

int VaultController::ownerFingerSlotForDevice(const QString &devicePath) const
{
    const QJsonObject reg = registryEntryForDevice(devicePath);
    if (reg.isEmpty())
        return -1;
    int owner = reg.value(QStringLiteral("ownerFingerId")).toInt(-1);
    if (owner >= 1)
        return owner;
    const QJsonArray list = reg.value(QStringLiteral("accessList")).toArray();
    for (const QJsonValue &v : list) {
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("isOwner")).toBool())
            return o.value(QStringLiteral("fingerId")).toInt(-1);
    }
    return -1;
}

int VaultController::nextFreeFingerSlot(const QString &excludeDevicePath) const
{
    QSet<int> used = collectUsedFingerSlots(excludeDevicePath);
    used.unite(m_sensorOccupiedSlots);
    for (int id = 1; id <= 127; ++id) {
        if (!used.contains(id))
            return id;
    }
    return -1;
}

void VaultController::loadSensorSlotCache()
{
    m_sensorOccupiedSlots.clear();
    const QStringList parts =
        QSettings().value(QStringLiteral("sensor/occupiedSlots")).toStringList();
    for (const QString &p : parts) {
        bool ok = false;
        const int id = p.toInt(&ok);
        if (ok && id >= 1 && id <= 127)
            m_sensorOccupiedSlots.insert(id);
    }
}

void VaultController::saveSensorSlotCache()
{
    QStringList sl;
    for (int id : m_sensorOccupiedSlots)
        sl.append(QString::number(id));
    QSettings().setValue(QStringLiteral("sensor/occupiedSlots"), sl);
}

void VaultController::markSensorSlot(int fingerId, bool occupied)
{
    if (fingerId < 1 || fingerId > 127)
        return;
    if (occupied)
        m_sensorOccupiedSlots.insert(fingerId);
    else
        m_sensorOccupiedSlots.remove(fingerId);
    saveSensorSlotCache();
}

QSet<int> VaultController::collectSensorOccupiedSlots() const
{
    return m_sensorOccupiedSlots;
}

bool VaultController::isSlotReferencedInRegistry(int fingerId) const
{
    if (fingerId < 1)
        return false;
    const QJsonArray arr = loadRegistryArray();
    for (const QJsonValue &v : arr) {
        const QJsonObject reg = v.toObject();
        if (reg.value(QStringLiteral("ownerFingerId")).toInt(-1) == fingerId)
            return true;
        const QJsonArray list = reg.value(QStringLiteral("accessList")).toArray();
        for (const QJsonValue &entry : list) {
            if (entry.toObject().value(QStringLiteral("fingerId")).toInt(-1) == fingerId)
                return true;
        }
    }
    return false;
}

bool VaultController::isOrphanSensorSlot(int fingerId) const
{
    return m_sensorOccupiedSlots.contains(fingerId) && !isSlotReferencedInRegistry(fingerId);
}

void VaultController::scheduleSensorInventoryRefresh()
{
    if (!m_inventoryRefreshTimer)
        return;
    m_inventoryRefreshTimer->start();
}

void VaultController::performSensorInventoryRefresh()
{
    /* Serial listEnrolled removed — firmware assigns slots on enroll(0). Simulator-only refresh. */
    if (m_fpUnlockActive || m_fpFlow != FpFlow::None)
        return;
    if (!QFile::exists(m_socketPath))
        return;

    QJsonObject cmd;
    cmd[QStringLiteral("cmd")] = QStringLiteral("list");
    const QByteArray resp = sendSocketCommand(m_socketPath, cmd);
    if (resp.isEmpty())
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(resp);
    const QJsonObject obj = doc.object();
    if (obj.value(QStringLiteral("status")).toString() != QStringLiteral("ok"))
        return;
    for (const QJsonValue &f : obj.value(QStringLiteral("fingers")).toArray()) {
        const int id = f.toObject().value(QStringLiteral("finger_id")).toInt(-1);
        if (id >= 1 && id <= 127)
            m_sensorOccupiedSlots.insert(id);
    }
    saveSensorSlotCache();
}

void VaultController::refreshSensorInventory()
{
    if (QFile::exists(m_socketPath))
        performSensorInventoryRefresh();
}

void VaultController::runExclusiveSerialOp(const std::function<void()> &action,
                                           bool preemptInventory)
{
    const int gen = ++m_serialExclusiveGeneration;
    runExclusiveSerialOpRetry(gen, action, preemptInventory, 0);
}

void VaultController::runExclusiveSerialOpRetry(int generation,
                                                const std::function<void()> &action,
                                                bool preemptInventory, int elapsedMs)
{
    if (generation != m_serialExclusiveGeneration)
        return;

    if (!m_serialBridge || !m_serialBridge->isConnected()) {
        if (m_fpUnlockActive)
            abortFingerprintOperationUi();
        emit notificationReceived(QStringLiteral("Fingerprint sensor not connected"), true);
        return;
    }

    if (m_serialBridge->isBusy()) {
        if (preemptInventory)
            m_serialBridge->cancelInventoryIfRunning();
        if (m_serialBridge->isBusy()) {
            constexpr int kMaxWaitMs = 3000;
            constexpr int kStepMs = 80;
            if (elapsedMs < kMaxWaitMs) {
                QTimer::singleShot(kStepMs, this, [this, generation, action, preemptInventory,
                                                   elapsedMs]() {
                    runExclusiveSerialOpRetry(generation, action, preemptInventory,
                                              elapsedMs + kStepMs);
                });
                return;
            }
            if (m_fpUnlockActive)
                abortFingerprintOperationUi();
            emit notificationReceived(
                QStringLiteral("Sensor is busy — try again in a moment"), true);
            return;
        }
    }

    action();
}

void VaultController::onListEnrolledCompleted(bool ok, const QList<int> &fingerIds,
                                              const QString &err)
{
    Q_UNUSED(err);
    if (!ok)
        return;
    m_sensorOccupiedSlots.clear();
    for (int id : fingerIds) {
        if (id >= 1 && id <= 127)
            m_sensorOccupiedSlots.insert(id);
    }
    saveSensorSlotCache();
}

void VaultController::rollbackSetupEnrollAfterVerifyFail(const QString &reason)
{
    if (m_setupOwnerFingerId < 1 || m_setupDevicePath.isEmpty())
        return;
    if (ownerFingerSlotForDevice(m_setupDevicePath) >= 1)
        return;

    const int slot = m_setupOwnerFingerId;
    m_setupOwnerFingerId = -1;
    emit setupOwnerFingerIdChanged();
    m_pendingSetupVerifyRollback = slot;

    if (m_serialBridge && m_serialBridge->isConnected() && !m_serialBridge->isBusy()) {
        m_serialBridge->deleteFinger(slot);
    } else {
        m_pendingSetupVerifyRollback = -1;
        markSensorSlot(slot, false);
    }
    emit notificationReceived(reason, true);
}

void VaultController::beginOwnerEnrollAuto()
{
    if (!m_serialBridge || !m_serialBridge->isConnected()) {
        if (m_fpUnlockActive)
            abortFingerprintOperationUi();
        emit notificationReceived(QStringLiteral("Fingerprint sensor not connected"), true);
        return;
    }

    const bool continuingEnroll =
        m_fpUnlockActive && m_fpFlow == FpFlow::SetupEnroll;
    if (!continuingEnroll && !ensureFingerprintIdle())
        return;

    if (!continuingEnroll)
        beginFingerprintOperationUi(FpFlow::SetupEnroll,
                                    QStringLiteral("Preparing sensor…"));

    runExclusiveSerialOp([this]() {
        m_fpFlow = FpFlow::SetupEnroll;
        emit fingerprintUnlockActiveChanged();
        if (!m_serialBridge->tryStartEnroll(0)) {
            abortFingerprintOperationUi();
            emit notificationReceived(
                QStringLiteral("Sensor is busy — please wait"), true);
            return;
        }
    });
}

void VaultController::onDeleteCompletedForEnroll(bool ok, int fingerId, const QString &err)
{
    if (!m_pendingDeleteBeforeEnroll || m_pendingEnrollSlot < 0)
        return;
    m_pendingDeleteBeforeEnroll = false;
    m_pendingEnrollSlot = -1;

    if (!ok) {
        m_pendingEnrollRetryAfterExists = false;
        const QString msg =
            err.isEmpty()
                ? QStringLiteral("Could not clear previous owner fingerprint on sensor")
                : err;
        setFingerprintPromptOutcome(2, msg);
        emit notificationReceived(msg, true);
        return;
    }

    markSensorSlot(fingerId, false);
    m_fpUnlockMessage = QStringLiteral("Preparing sensor…");
    emit fingerprintUnlockActiveChanged();
    beginOwnerEnrollAuto();
}

void VaultController::beginSetupOwnerEnrollForDevice(const QString &devicePath)
{
    const QString path = devicePath.trimmed();
    if (path.isEmpty()) {
        emit notificationReceived(QStringLiteral("No device selected for setup"), true);
        return;
    }
    m_setupDevicePath = path;

    if (!m_serialBridge->isConnected()) {
        if (!ensureFingerprintIdle())
            return;
        beginFingerprintOperationUi(FpFlow::SetupFindExisting,
                                    QStringLiteral("Connecting to sensor…"));
        tryConnectSerial();
        QTimer::singleShot(500, this, [this, path]() {
            beginSetupOwnerEnrollForDeviceAfterConnect(path);
        });
        return;
    }
    beginSetupOwnerEnrollForDeviceAfterConnect(path);
}

void VaultController::beginSetupOwnerEnrollForDeviceAfterConnect(const QString &devicePath)
{
    if (!m_serialBridge || !m_serialBridge->isConnected()) {
        if (m_fpUnlockActive)
            abortFingerprintOperationUi();
        emit notificationReceived(QStringLiteral("Fingerprint sensor not connected"), true);
        return;
    }

    const QString path = devicePath.trimmed();
    m_setupDevicePath = path;

    int existing = ownerFingerSlotForDevice(path);
    if (existing < 1 && m_setupOwnerFingerId >= 1)
        existing = m_setupOwnerFingerId;

    if (existing >= 1) {
        m_pendingEnrollSlot = existing;
        m_pendingDeleteBeforeEnroll = true;
        const bool continuingEnroll =
            m_fpUnlockActive && m_fpFlow == FpFlow::SetupEnroll;
        if (!continuingEnroll && !ensureFingerprintIdle())
            return;
        if (!continuingEnroll)
            beginFingerprintOperationUi(FpFlow::SetupEnroll,
                                        QStringLiteral("Clearing previous fingerprint…"));
        runExclusiveSerialOp([this, existing]() {
            if (!m_serialBridge->tryDeleteFinger(existing)) {
                m_pendingDeleteBeforeEnroll = false;
                m_pendingEnrollSlot = -1;
                abortFingerprintOperationUi();
                emit notificationReceived(
                    QStringLiteral("Sensor is busy — please wait"), true);
            }
        });
        return;
    }

    beginSetupFindExisting();
}

void VaultController::beginSetupFindExisting()
{
    if (!m_serialBridge || !m_serialBridge->isConnected()) {
        if (m_fpUnlockActive)
            abortFingerprintOperationUi();
        emit notificationReceived(QStringLiteral("Fingerprint sensor not connected"), true);
        return;
    }
    if (!ensureFingerprintIdle())
        return;

    beginFingerprintOperationUi(
        FpFlow::SetupFindExisting,
        QStringLiteral("Touch the owner finger — an existing print will be reused if found"));
    runExclusiveSerialOp([this]() {
        m_fpFlow = FpFlow::SetupFindExisting;
        m_fpUnlockMessage =
            QStringLiteral("Touch the owner finger — an existing print will be reused if found");
        emit fingerprintUnlockActiveChanged();
        if (!m_serialBridge->tryStartVerify()) {
            abortFingerprintOperationUi();
            emit notificationReceived(
                QStringLiteral("Sensor is busy — please wait"), true);
        }
    });
}

void VaultController::beginSetupEnrollAfterNoMatch()
{
    if (!m_serialBridge || !m_serialBridge->isConnected()) {
        abortFingerprintOperationUi();
        emit notificationReceived(QStringLiteral("Fingerprint sensor not connected"), true);
        return;
    }

    m_fpFlow = FpFlow::SetupEnroll;
    m_fpPromptState = 0;
    m_fpEnrollStep = 0;
    m_fpUnlockMessage =
        QStringLiteral("No matching print — place your finger to enroll a new one");
    m_fpUnlockActive = true;
    emit fingerprintUnlockActiveChanged();
    emit fingerprintPromptStateChanged();
    emit fingerprintEnrollStepChanged();

    runExclusiveSerialOp([this]() {
        m_fpFlow = FpFlow::SetupEnroll;
        if (!m_serialBridge->tryStartEnroll(0)) {
            abortFingerprintOperationUi();
            emit notificationReceived(
                QStringLiteral("Sensor is busy — please wait"), true);
        }
    });
}

QObject *VaultController::serialBridgeObject() const
{
    return m_serialBridge;
}

QString VaultController::fingerprintFlow() const
{
    switch (m_fpFlow) {
    case FpFlow::Unlock:
        return QStringLiteral("unlock");
    case FpFlow::SetupFindExisting:
        return QStringLiteral("setup_find_existing");
    case FpFlow::SetupEnroll:
        return QStringLiteral("setup_enroll");
    case FpFlow::SetupVerify:
        return QStringLiteral("setup_verify");
    case FpFlow::GrantVerify:
        return QStringLiteral("grant_verify");
    case FpFlow::GrantEnroll:
        return QStringLiteral("grant_enroll");
    case FpFlow::RevokeVerify:
        return QStringLiteral("revoke_verify");
    case FpFlow::DeleteVerify:
        return QStringLiteral("delete_verify");
    default:
        return QString();
    }
}

void VaultController::setSetupOwnerLabel(const QString &label)
{
    const QString trimmed = label.trimmed();
    if (m_setupOwnerLabel == trimmed)
        return;
    m_setupOwnerLabel = trimmed;
    emit setupOwnerLabelChanged();
}

bool VaultController::ensureFingerprintIdle()
{
    if (m_fpUnlockActive || m_fpFlow != FpFlow::None) {
        emit notificationReceived(
            QStringLiteral("Another fingerprint operation is in progress. Cancel it or wait."),
            true);
        return false;
    }
    return true;
}

void VaultController::cleanupGrantEnrollOrphan()
{
    if (m_fpFlow != FpFlow::GrantEnroll || m_pendingGrantFingerId < 1)
        return;
    const int fingerId = m_pendingGrantFingerId;
    if (m_serialBridge && m_serialBridge->isConnected() && !m_serialBridge->isBusy())
        m_serialBridge->deleteFinger(fingerId);
    else
        markSensorSlot(fingerId, false);
}

void VaultController::clearFingerprintFlowState()
{
    m_fpFlow = FpFlow::None;
    m_fpContextDevice.clear();
    m_pendingGrantOwnerId = -1;
    m_pendingGrantFingerId = -1;
    m_pendingGrantLabel.clear();
    m_pendingRevokeFingerId = -1;
    m_pendingDeleteDevicePath.clear();
}

bool VaultController::isLiveFingerprintFlow(FpFlow flow)
{
    switch (flow) {
    case FpFlow::SetupFindExisting:
    case FpFlow::SetupEnroll:
    case FpFlow::GrantEnroll:
    case FpFlow::Unlock:
    case FpFlow::SetupVerify:
    case FpFlow::GrantVerify:
    case FpFlow::RevokeVerify:
    case FpFlow::DeleteVerify:
        return true;
    default:
        return false;
    }
}

void VaultController::beginFingerprintOperationUi(FpFlow flow, const QString &message)
{
    m_fpFlow = flow;
    m_fpUnlockMessage = message;
    m_fpPromptState = 0;
    m_fpEnrollStep = 0;
    m_fpUnlockActive = true;
    emit fingerprintUnlockActiveChanged();
    emit fingerprintPromptStateChanged();
    emit fingerprintEnrollStepChanged();
}

void VaultController::abortFingerprintOperationUi()
{
    m_fpUnlockActive = false;
    m_fpUnlockMessage.clear();
    m_fpPromptState = 0;
    m_fpEnrollStep = 0;
    clearFingerprintFlowState();
    emit fingerprintUnlockActiveChanged();
    emit fingerprintPromptStateChanged();
    emit fingerprintEnrollStepChanged();
}

void VaultController::endFingerprintOperationUi()
{
    m_fpUnlockActive = false;
    m_fpUnlockMessage.clear();
    m_fpPromptState = 0;
    m_fpEnrollStep = 0;
    clearFingerprintFlowState();
    emit fingerprintUnlockActiveChanged();
    emit fingerprintPromptStateChanged();
    emit fingerprintEnrollStepChanged();
}

void VaultController::setFingerprintPromptOutcome(int state, const QString &message)
{
    m_fpPromptState = state;
    if (!message.isEmpty())
        m_fpUnlockMessage = message;
    emit fingerprintUnlockActiveChanged();
    emit fingerprintPromptStateChanged();
}

bool VaultController::isFingerAllowedForVault(const QString &devicePath, int fingerId) const
{
    if (devicePath.isEmpty() || fingerId < 1)
        return false;
    const QJsonObject reg = registryEntryForDevice(devicePath);
    const QJsonArray list = reg.value(QStringLiteral("accessList")).toArray();
    if (list.isEmpty()) {
        const int owner = reg.value(QStringLiteral("ownerFingerId")).toInt(-1);
        return owner == fingerId;
    }
    for (const QJsonValue &v : list) {
        if (v.toObject().value(QStringLiteral("fingerId")).toInt() == fingerId)
            return true;
    }
    return false;
}

bool VaultController::labelExistsForVault(const QString &devicePath, const QString &label) const
{
    const QString want = label.trimmed();
    if (devicePath.isEmpty() || want.isEmpty())
        return false;
    const QJsonObject reg = registryEntryForDevice(devicePath);
    const QJsonArray list = reg.value(QStringLiteral("accessList")).toArray();
    for (const QJsonValue &v : list) {
        const QString existing = v.toObject().value(QStringLiteral("label")).toString().trimmed();
        if (existing.compare(want, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

void VaultController::handleSensorEvent(int eventCode)
{
    if (!m_fpUnlockActive && !isLiveFingerprintFlow(m_fpFlow))
        return;

    if (!m_fpUnlockActive && isLiveFingerprintFlow(m_fpFlow))
        m_fpUnlockActive = true;

    const bool enrollFlow =
        m_fpFlow == FpFlow::SetupEnroll || m_fpFlow == FpFlow::GrantEnroll;
    const bool setupVerify = m_fpFlow == FpFlow::SetupVerify;
    const bool setupFindExisting = m_fpFlow == FpFlow::SetupFindExisting;

    switch (eventCode) {
    case 0x10:
        if (setupVerify)
            m_fpUnlockMessage =
                QStringLiteral("Touch the sensor once to confirm setup");
        else if (setupFindExisting)
            m_fpUnlockMessage =
                QStringLiteral("Touch the owner finger — existing prints are checked first");
        else
            m_fpUnlockMessage = QStringLiteral("Place your finger on the sensor");
        if (enrollFlow) {
            m_fpEnrollStep = 1;
            emit fingerprintEnrollStepChanged();
        }
        break;
    case 0x11:
        m_fpUnlockMessage = setupVerify
                                ? QStringLiteral("Hold still…")
                                : QStringLiteral("Finger detected — hold still");
        break;
    case 0x12:
        if (!enrollFlow)
            break;
        m_fpUnlockMessage = QStringLiteral("Remove your finger from the sensor");
        break;
    case 0x13:
        if (!enrollFlow)
            break;
        m_fpUnlockMessage = QStringLiteral("Place the same finger again");
        m_fpEnrollStep = 2;
        emit fingerprintEnrollStepChanged();
        break;
    case 0x14:
        if (setupVerify)
            m_fpUnlockMessage = QStringLiteral("Confirming…");
        else if (setupFindExisting)
            m_fpUnlockMessage = QStringLiteral("Checking fingerprint…");
        else
            m_fpUnlockMessage = QStringLiteral("Processing fingerprint…");
        break;
    default:
        return;
    }
    emit fingerprintUnlockActiveChanged();
}

void VaultController::startAuthorizedVerify(FpFlow flow, const QString &devicePath,
                                            const QString &initialMessage)
{
    if (!ensureFingerprintIdle())
        return;

    if (!m_serialBridge || !m_serialBridge->isConnected()) {
        emit notificationReceived(QStringLiteral("Fingerprint sensor not connected"), true);
        return;
    }

    beginFingerprintOperationUi(flow, QStringLiteral("Preparing sensor…"));
    m_fpContextDevice = devicePath;
    runExclusiveSerialOp([this, flow, devicePath, initialMessage]() {
        m_fpFlow = flow;
        m_fpContextDevice = devicePath;
        m_fpUnlockMessage = initialMessage;
        emit fingerprintUnlockActiveChanged();
        if (!m_serialBridge->tryStartVerify()) {
            abortFingerprintOperationUi();
            emit notificationReceived(
                QStringLiteral("Sensor is busy — please wait"), true);
            return;
        }
    });
}

namespace {

bool verifyFailureShouldStartSetupEnroll(const QString &err)
{
    if (err.isEmpty())
        return true;
    if (err.contains(QStringLiteral("eşleşmedi"), Qt::CaseInsensitive))
        return true;
    if (err.contains(QStringLiteral("no match"), Qt::CaseInsensitive))
        return true;
    if (err.contains(QStringLiteral("did not match"), Qt::CaseInsensitive))
        return true;
    if (err.contains(QStringLiteral("parmak algılanmadı"), Qt::CaseInsensitive))
        return true;
    if (err.contains(QStringLiteral("no finger"), Qt::CaseInsensitive))
        return true;
    return false;
}

} // namespace

void VaultController::handleVerifyCompleted(bool ok, int matchedId, const QString &err)
{
    if (!m_fpUnlockActive)
        return;

    if (m_fpFlow == FpFlow::SetupFindExisting) {
        if (ok && matchedId >= 1) {
            m_setupOwnerFingerId = matchedId;
            /* Reused slot: require verify again at Start Setup (no post-enroll key skip). */
            m_setupOwnerEnrolledThisSession = false;
            m_setupEnrollKeyGraceUntilMs = 0;
            markSensorSlot(matchedId, true);
            emit setupOwnerFingerIdChanged();
            endFingerprintOperationUi();
            emit notificationReceived(
                QStringLiteral("Existing fingerprint reused as owner (slot %1).")
                    .arg(matchedId),
                false);
            return;
        }
        if (!ok && verifyFailureShouldStartSetupEnroll(err)) {
            beginSetupEnrollAfterNoMatch();
            return;
        }
        m_fpUnlockMessage =
            err.isEmpty() ? QStringLiteral("Could not check fingerprint") : err;
        m_fpUnlockActive = false;
        clearFingerprintFlowState();
        emit fingerprintUnlockActiveChanged();
        emit notificationReceived(m_fpUnlockMessage, true);
        return;
    }

    if (m_fpFlow == FpFlow::SetupVerify) {
        if (!ok) {
            m_fpUnlockMessage =
                err.isEmpty() ? QStringLiteral("Kurulum doğrulaması başarısız") : err;
            m_fpUnlockActive = false;
            clearFingerprintFlowState();
            emit fingerprintUnlockActiveChanged();
            rollbackSetupEnrollAfterVerifyFail(
                QStringLiteral("Kurulum doğrulaması başarısız; kayıt temizlendi. "
                               "Sahip parmağını yeniden kaydedin."));
            return;
        }
        if (matchedId != m_setupOwnerFingerId) {
            m_fpUnlockMessage =
                QStringLiteral("Kayıt slot #%1 ile doğrulama slot #%2 uyuşmuyor. "
                               "Kayıt sırasındaki parmağı kullanın.")
                    .arg(m_setupOwnerFingerId)
                    .arg(matchedId);
            m_fpUnlockActive = false;
            clearFingerprintFlowState();
            emit fingerprintUnlockActiveChanged();
            rollbackSetupEnrollAfterVerifyFail(m_fpUnlockMessage);
            return;
        }
        m_fpUnlockMessage = QStringLiteral("Kasa anahtarı alınıyor…");
        emit fingerprintUnlockActiveChanged();
        m_serialBridge->requestKey();
        return;
    }

    if (m_fpFlow == FpFlow::GrantVerify) {
        const QString device = m_fpContextDevice;
        const QString label = m_pendingGrantLabel;
        if (!ok) {
            m_fpUnlockMessage =
                err.isEmpty() ? QStringLiteral("Authorized verification failed") : err;
            m_fpUnlockActive = false;
            clearFingerprintFlowState();
            emit fingerprintUnlockActiveChanged();
            emit notificationReceived(m_fpUnlockMessage, true);
            return;
        }
        if (!isFingerAllowedForVault(device, matchedId)) {
            m_fpUnlockMessage =
                QStringLiteral("This fingerprint is not authorized for this vault.");
            m_fpUnlockActive = false;
            clearFingerprintFlowState();
            emit fingerprintUnlockActiveChanged();
            emit notificationReceived(m_fpUnlockMessage, true);
            return;
        }
        m_fpUnlockActive = false;
        m_fpFlow = FpFlow::None;
        emit fingerprintUnlockActiveChanged();
        startGrantEnrollForVault(device, label);
        return;
    }

    if (m_fpFlow == FpFlow::RevokeVerify) {
        const QString device = m_fpContextDevice;
        const int fingerId = m_pendingRevokeFingerId;
        if (!ok) {
            m_fpUnlockMessage =
                err.isEmpty() ? QStringLiteral("Authorized verification failed") : err;
            m_fpUnlockActive = false;
            clearFingerprintFlowState();
            emit fingerprintUnlockActiveChanged();
            emit notificationReceived(m_fpUnlockMessage, true);
            return;
        }
        if (!isFingerAllowedForVault(device, matchedId)) {
            m_fpUnlockMessage =
                QStringLiteral("This fingerprint is not authorized for this vault.");
            m_fpUnlockActive = false;
            clearFingerprintFlowState();
            emit fingerprintUnlockActiveChanged();
            emit notificationReceived(m_fpUnlockMessage, true);
            return;
        }
        m_fpUnlockActive = false;
        clearFingerprintFlowState();
        emit fingerprintUnlockActiveChanged();
        finishRevokeAfterGate(device, fingerId);
        return;
    }

    if (m_fpFlow == FpFlow::DeleteVerify) {
        const QString device = m_pendingDeleteDevicePath;
        if (!ok) {
            m_fpUnlockMessage =
                err.isEmpty() ? QStringLiteral("Vault deletion verification failed") : err;
            m_fpUnlockActive = false;
            clearFingerprintFlowState();
            emit fingerprintUnlockActiveChanged();
            emit notificationReceived(m_fpUnlockMessage, true);
            return;
        }
        if (!isFingerAllowedForVault(device, matchedId)) {
            m_fpUnlockMessage =
                QStringLiteral("An authorized fingerprint is required to delete this vault.");
            m_fpUnlockActive = false;
            clearFingerprintFlowState();
            emit fingerprintUnlockActiveChanged();
            emit notificationReceived(m_fpUnlockMessage, true);
            return;
        }
        m_fpUnlockActive = false;
        m_pendingDeleteDevicePath.clear();
        clearFingerprintFlowState();
        emit fingerprintUnlockActiveChanged();
        emit deleteVaultGatePassed(device);
        return;
    }

    if (m_fpFlow != FpFlow::Unlock)
        return;

    if (!ok) {
        m_fpUnlockMessage = err.isEmpty() ? QStringLiteral("Fingerprint did not match") : err;
        m_fpUnlockActive = false;
        clearFingerprintFlowState();
        emit fingerprintUnlockActiveChanged();
        emit notificationReceived(m_fpUnlockMessage, true);
        return;
    }
    if (!isFingerAllowedForSelectedVault(matchedId)) {
        m_fpUnlockMessage = QStringLiteral("You are not authorized for this vault.");
        m_fpUnlockActive = false;
        clearFingerprintFlowState();
        emit fingerprintUnlockActiveChanged();
        emit notificationReceived(m_fpUnlockMessage, true);
        return;
    }
    m_fpUnlockMessage = QStringLiteral("Anahtar alınıyor…");
    emit fingerprintUnlockActiveChanged();
    m_serialBridge->requestKey();
}

bool VaultController::appendAccessListEntry(const QString &devicePath, int fingerId,
                                              const QString &label, bool isOwner)
{
    if (devicePath.isEmpty() || fingerId < 1)
        return false;

    QJsonObject reg = registryEntryForDevice(devicePath);
    if (reg.isEmpty())
        reg.insert(QStringLiteral("path"), devicePath);

    QJsonArray list = reg.value(QStringLiteral("accessList")).toArray();
    for (const QJsonValue &v : list) {
        if (v.toObject().value(QStringLiteral("fingerId")).toInt() == fingerId) {
            emit notificationReceived(QStringLiteral("This slot is already registered for this vault"), true);
            return false;
        }
    }
    if (!isOwner && list.size() >= 4) {
        emit notificationReceived(QStringLiteral("Maximum of 4 access entries per vault"), true);
        return false;
    }

    QJsonObject entry;
    entry.insert(QStringLiteral("fingerId"), fingerId);
    entry.insert(QStringLiteral("isOwner"), isOwner);
    entry.insert(QStringLiteral("label"),
                 label.trimmed().isEmpty() ? QStringLiteral("Kullanıcı %1").arg(fingerId)
                                           : label.trimmed());
    list.append(entry);
    reg.insert(QStringLiteral("accessList"), list);
    if (isOwner)
        reg.insert(QStringLiteral("ownerFingerId"), fingerId);

    if (!saveRegistryEntry(reg)) {
        emit notificationReceived(QStringLiteral("Could not update vault registry"), true);
        return false;
    }
    markSensorSlot(fingerId, true);
    return true;
}

void VaultController::startGrantEnrollForVault(const QString &devicePath, const QString &label)
{
    if (!ensureFingerprintIdle())
        return;

    if (!m_serialBridge || !m_serialBridge->isConnected()) {
        emit notificationReceived(QStringLiteral("Fingerprint sensor not connected"), true);
        return;
    }

    const QString trimmedLabel = label.trimmed();
    m_fpContextDevice = devicePath;
    m_pendingGrantFingerId = -1;
    m_pendingGrantLabel = trimmedLabel;
    beginFingerprintOperationUi(FpFlow::GrantEnroll, QStringLiteral("Preparing sensor…"));
    runExclusiveSerialOp([this, devicePath, trimmedLabel]() {
        m_fpContextDevice = devicePath;
        m_pendingGrantLabel = trimmedLabel;
        m_fpFlow = FpFlow::GrantEnroll;
        emit fingerprintUnlockActiveChanged();
        if (!m_serialBridge->tryStartEnroll(0)) {
            abortFingerprintOperationUi();
            emit notificationReceived(
                QStringLiteral("Sensor is busy — please wait"), true);
            return;
        }
    });
}

void VaultController::beginGrantAccessForVault(const QString &devicePath, const QString &label)
{
    if (!ensureFingerprintIdle())
        return;

    const QString path = devicePath.trimmed();
    const QString trimmedLabel = label.trimmed();
    if (path.isEmpty()) {
        emit notificationReceived(QStringLiteral("No vault selected"), true);
        return;
    }
    if (trimmedLabel.isEmpty()) {
        emit notificationReceived(QStringLiteral("Enter a display name for this fingerprint"), true);
        return;
    }
    if (labelExistsForVault(path, trimmedLabel)) {
        emit notificationReceived(
            QStringLiteral("Bu kasada \"%1\" adı zaten kullanılıyor").arg(trimmedLabel), true);
        return;
    }

    m_pendingGrantLabel = trimmedLabel;
    m_fpContextDevice = path;

    if (isVaultUnlockedForDevice(path)) {
        startGrantEnrollForVault(path, trimmedLabel);
        return;
    }

    startAuthorizedVerify(FpFlow::GrantVerify, path,
                          QStringLiteral("Place an authorized finger to grant access…"));
}

void VaultController::beginRevokeAccessForVault(const QString &devicePath, int fingerId)
{
    if (!ensureFingerprintIdle())
        return;

    const QString path = devicePath.trimmed();
    if (path.isEmpty() || fingerId < 1) {
        emit notificationReceived(QStringLiteral("Invalid operation"), true);
        return;
    }

    const QJsonObject reg = registryEntryForDevice(path);
    const QJsonArray list = reg.value(QStringLiteral("accessList")).toArray();
    for (const QJsonValue &v : list) {
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("fingerId")).toInt() == fingerId
            && o.value(QStringLiteral("isOwner")).toBool()) {
            emit notificationReceived(QStringLiteral("The vault owner cannot be removed"), true);
            return;
        }
    }

    m_pendingRevokeFingerId = fingerId;
    m_fpContextDevice = path;

    if (isVaultUnlockedForDevice(path)) {
        finishRevokeAfterGate(path, fingerId);
        return;
    }

    startAuthorizedVerify(FpFlow::RevokeVerify, path,
                          QStringLiteral("Place an authorized finger to remove access…"));
}

void VaultController::finishRevokeAfterGate(const QString &devicePath, int fingerId)
{
    QJsonObject reg = registryEntryForDevice(devicePath);
    QJsonArray list = reg.value(QStringLiteral("accessList")).toArray();
    QJsonArray next;
    bool removed = false;
    for (const QJsonValue &v : list) {
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("fingerId")).toInt() == fingerId) {
            if (o.value(QStringLiteral("isOwner")).toBool()) {
                emit notificationReceived(QStringLiteral("The vault owner cannot be removed"), true);
                return;
            }
            removed = true;
            continue;
        }
        next.append(o);
    }
    if (!removed) {
        emit notificationReceived(QStringLiteral("Entry not found"), true);
        return;
    }
    reg.insert(QStringLiteral("accessList"), next);
    if (!saveRegistryEntry(reg)) {
        emit notificationReceived(QStringLiteral("Could not update vault registry"), true);
        return;
    }

    if (m_serialBridge && m_serialBridge->isConnected() && !m_serialBridge->isBusy())
        m_serialBridge->deleteFinger(fingerId);
    else
        markSensorSlot(fingerId, false);

    emit notificationReceived(
        QStringLiteral("Access removed."), false);
    emit statusChanged();
    emit devicesChanged();
    emit vaultAccessListChanged();
}

void VaultController::beginDeleteVaultBiometricGate(const QString &devicePath)
{
    if (!ensureFingerprintIdle())
        return;

    const QString path = devicePath.trimmed();
    if (path.isEmpty()) {
        emit notificationReceived(QStringLiteral("No vault selected"), true);
        return;
    }

    if (isVaultUnlockedForDevice(path)) {
        emit deleteVaultGatePassed(path);
        return;
    }

    m_pendingDeleteDevicePath = path;
    startAuthorizedVerify(FpFlow::DeleteVerify, path,
                          QStringLiteral("Place an authorized finger to delete this vault…"));
}

VaultController::~VaultController()
{
    if (isVaultRuntimeOpen()) {
        runVaultManager({QStringLiteral("forceclose"), m_mountPoint}, 20000);
        m_isLocked = true;
        m_unlockedDevice.clear();
    }
    if (m_simProcess && m_simProcess->state() == QProcess::Running) {
        m_simProcess->terminate();
        m_simProcess->waitForFinished(3000);
    }
}

void VaultController::updateActivity(const QString &msg)
{
    m_lastActivity = QDateTime::currentDateTime().toString("hh:mm:ss") + " — " + msg;
}

/* ─── vault_manager Helper ───────────────────────────────────────────────── */

QString VaultController::findVaultManager() const
{
    /* 1. Build dizinindeki bin/ klasörüne bak */
    QString bin = QCoreApplication::applicationDirPath() + "/vault_manager";
    if (QFile::exists(bin)) return bin;

    /* 2. Proje dizininde ara */
    bin = m_projectDir + "/vault_manager/vault_manager";
    if (QFile::exists(bin)) return bin;

    /* 3. Build dizininin altındaki bin/ klasörüne bak */
    bin = m_projectDir + "/build/bin/vault_manager";
    if (QFile::exists(bin)) return bin;

    /* 4. Sistem PATH'inde ara */
    return "vault_manager";
}

/**
 * vault_manager binary'sini pkexec ile çalıştır ve sonucunu döndür.
 * pkexec, grafik ortamda bir şifre diyaloğu gösterir — terminal gerekmez.
 * Eğer pkexec yoksa sudo'ya düşer (fallback).
 *
 * @return Process exit code (0 = başarılı)
 */
int VaultController::runVaultManager(const QStringList &args, int timeoutMs)
{
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);

    /*
     * Ortam değişkenlerini hazırla:
     *  - SUDO_UID / SUDO_GID: vault_manager mount sonrası chown için kullanır
     *  - DISPLAY / XAUTHORITY: pkexec grafik diyaloğu için gerekli
     */
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("SUDO_UID", QString::number(getuid()));
    env.insert("SUDO_GID", QString::number(getgid()));
    proc.setProcessEnvironment(env);

    /*
     * pkexec kullan → grafik şifre diyaloğu
     * PolicyKit policy: com.digitalvault.manager.run
     */
    QString launcher = "pkexec";
    QStringList launchArgs;
    launchArgs << m_vaultManagerBin << args;

    /* pkexec yoksa sudo'ya düş */
    QProcess which;
    which.start("which", QStringList() << "pkexec");
    which.waitForFinished(2000);
    if (which.exitCode() != 0) {
        launcher = "sudo";
        addLog("WARNING", "pkexec not found, falling back to sudo");
    }

    printf("[GUI] Running: %s %s\n",
           launcher.toStdString().c_str(),
           launchArgs.join(" ").toStdString().c_str());

    proc.start(launcher, launchArgs);

    if (!proc.waitForStarted(5000)) {
        qWarning() << "[GUI] Failed to start vault_manager!";
        addLog("ERROR", "Could not launch vault_manager with " + launcher);
        return -1;
    }

    if (!proc.waitForFinished(timeoutMs)) {
        qWarning() << "[GUI] vault_manager timed out!";
        proc.kill();
        return -1;
    }

    /* vault_manager çıktılarını logla */
    QString output = QString::fromUtf8(proc.readAll()).trimmed();
    if (!output.isEmpty()) {
        for (const QString &line : output.split('\n')) {
            if (line.contains("Fingerprint verified")) {
                addLog("SUCCESS", "Biometric verification passed");
            } else if (line.contains("key wiped", Qt::CaseInsensitive)) {
                addLog("INFO", "Encryption key cleared from memory");
            } else if (line.contains("failed", Qt::CaseInsensitive)) {
                addLog("ERROR", line.trimmed());
            }
            printf("[vault_manager] %s\n", line.toStdString().c_str());
        }
    }

    return proc.exitCode();
}

/* ─── Simulator Management ───────────────────────────────────────────────── */

void VaultController::startSimulator()
{
    const QString uartDevice = QString::fromUtf8(kDefaultUartDevice);
    const QString uartHost = QString::fromUtf8(kDefaultUartHost);

    if (m_simProcess && m_simProcess->state() == QProcess::Running) {
        QTimer::singleShot(200, this, [this, uartHost]() { tryConnectSerial(); });
        emit notificationReceived(
            QStringLiteral("Simülatör zaten çalışıyor — UART bağlanıyor…"), false);
        emit statusChanged();
        return;
    }

    if (QFile::exists(m_socketPath) && QFile::exists(uartHost)) {
        QTimer::singleShot(500, this, [this]() { tryConnectSerial(); });
        updateActivity(QStringLiteral("Harici simülatör — UART bağlanıyor"));
        emit statusChanged();
        emit notificationReceived(
            QStringLiteral("Harici simülatör bulundu — yalnızca UART bağlanıyor (ikinci sim yok)"),
            false);
        QTimer::singleShot(1000, this, &VaultController::refreshFingerprints);
        return;
    }

    m_simProcess = new QProcess(this);
    connect(m_simProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &VaultController::onSimFinished);

    QString simPath = QCoreApplication::applicationDirPath() + "/simulator.py";
    if (!QFile::exists(simPath)) {
        simPath = m_projectDir + "/hw_simulator/simulator.py";
    }

    if (!QFile::exists(simPath)) {
        emit notificationReceived("Simulator not found: " + simPath, true);
        return;
    }

    if (!QFile::exists(uartHost)) {
        m_socatProcess = new QProcess(this);
        m_socatProcess->setProgram(QStringLiteral("socat"));
        m_socatProcess->setArguments({
            QStringLiteral("pty,link=%1,raw,echo=0").arg(uartDevice),
            QStringLiteral("pty,link=%1,raw,echo=0").arg(uartHost),
        });
        m_socatProcess->start();
        if (!m_socatProcess->waitForStarted(2000)) {
            emit notificationReceived(QStringLiteral("socat failed — install socat for UART bridge"), true);
            return;
        }
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("BIOMETRIC_UART"), uartDevice);
    m_simProcess->setProcessEnvironment(env);
    m_simProcess->start(QStringLiteral("python3"), QStringList() << simPath << QStringLiteral("-v"));

    if (!m_simProcess->waitForStarted(3000)) {
        emit notificationReceived("Failed to start simulator!", true);
        return;
    }

    QTimer::singleShot(1200, this, [this]() { tryConnectSerial(); });

    updateActivity("Hardware simulator started.");
    emit statusChanged();
    emit notificationReceived("Simulator started successfully.", false);

    QTimer::singleShot(1000, this, &VaultController::refreshFingerprints);
}

void VaultController::stopSimulator()
{
    if (!m_simProcess || m_simProcess->state() != QProcess::Running) {
        emit notificationReceived("Simulator is not running.", false);
        return;
    }

    if (isVaultRuntimeOpen())
        emergencyClose();

    m_simProcess->terminate();
    if (!m_simProcess->waitForFinished(3000)) {
        m_simProcess->kill();
    }

    updateActivity("Simulator stopped.");
    emit statusChanged();
    emit notificationReceived("Simulator stopped.", false);
}

void VaultController::onSimFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    (void)exitCode; (void)exitStatus;
    if (isVaultRuntimeOpen() && !m_isToggling)
        emergencyClose();
    updateActivity("Simulator process ended.");
    emit statusChanged();
}

/* ─── Vault Mount/Unmount (LUKS + ext4) ──────────────────────────────────── */

bool VaultController::mountVault()
{
    addLog("INFO", "Starting vault unlock for: " + m_selectedDevice);

    int ret = runVaultManager({"open", m_selectedDevice, m_mountPoint}, 15000);

    if (ret == 0) {
        addLog("SUCCESS", "Vault decrypted and mounted at " + m_mountPoint);

        /*
         * Mount başarılı — dosya yöneticisinde aç.
         * Bu sayede kullanıcı hemen dosyalarına erişebilir.
         * xdg-open ile varsayılan dosya yöneticisi açılır (Nautilus, Dolphin, Thunar vb.)
         */
        QProcess::startDetached("xdg-open", {m_mountPoint});

        return true;
    }

    addLog("ERROR", "Failed to open vault (exit code: " + QString::number(ret) + ")");
    return false;
}

bool VaultController::unmountVault()
{
    addLog("INFO", "Locking vault and clearing kernel keys...");

    int ret = runVaultManager({"close", m_mountPoint}, 10000);

    if (ret == 0) {
        addLog("SUCCESS", "Vault locked. Encryption keys cleared from kernel.");
        return true;
    }

    if (ret == 2) {
        addLog("WARNING", "Mount point is busy. Close file manager or terminal windows using the vault.");
        emit notificationReceived(
            "Could not lock vault: mount point is busy. Close apps using the mount folder.", true);
    } else {
        addLog("ERROR", "Failed to close vault (exit code: " + QString::number(ret) + ")");
    }
    return false;
}

bool VaultController::isFingerAllowedForSelectedVault(int fingerId) const
{
    return isFingerAllowedForVault(m_selectedDevice, fingerId);
}

bool VaultController::fingerInAccessList(int fingerId) const
{
    return isFingerAllowedForSelectedVault(fingerId);
}

int VaultController::runVaultManagerWithKey(const QStringList &args,
                                            const QByteArray &key32,
                                            int timeoutMs)
{
    m_lastVaultManagerOutput.clear();
    if (key32.size() != 32)
        return -1;

    QTemporaryFile keyFile;
    keyFile.setAutoRemove(true);
    if (!keyFile.open()) {
        m_lastVaultManagerOutput = QStringLiteral("Could not create temporary key file");
        return -1;
    }
    keyFile.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    if (keyFile.write(key32) != 32) {
        m_lastVaultManagerOutput = QStringLiteral("Could not write vault key");
        return -1;
    }
    keyFile.flush();
    const QString keyPath = keyFile.fileName();
    keyFile.close();

    QStringList vmArgs;
    for (const QString &arg : args) {
        if (arg == QStringLiteral("--key-from-stdin"))
            continue;
        vmArgs << arg;
    }
    vmArgs << QStringLiteral("--key-file") << keyPath;

    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("SUDO_UID"), QString::number(getuid()));
    env.insert(QStringLiteral("SUDO_GID"), QString::number(getgid()));
    proc.setProcessEnvironment(env);

    QStringList launchArgs;
    launchArgs << m_vaultManagerBin << vmArgs;
    proc.start(QStringLiteral("pkexec"), launchArgs);
    if (!proc.waitForStarted(5000)) {
        m_lastVaultManagerOutput = QStringLiteral("pkexec failed to start vault_manager");
        return -1;
    }

    /* pkexec does not reliably forward stdin to the child; pass key via --key-file. */
    if (!proc.waitForFinished(timeoutMs)) {
        proc.kill();
        m_lastVaultManagerOutput = QStringLiteral("vault_manager timed out");
        return -1;
    }
    m_lastVaultManagerOutput = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    return proc.exitCode();
}

void VaultController::beginFingerprintUnlock()
{
    if (m_selectedDevice.isEmpty()) {
        emit notificationReceived(QStringLiteral("Önce bir kasa seçin"), true);
        return;
    }
    if (!m_serialBridge->isConnected()) {
        tryConnectSerial();
        QTimer::singleShot(500, this, &VaultController::beginFingerprintUnlockAfterConnect);
        return;
    }
    if (!ensureFingerprintIdle())
        return;

    beginFingerprintOperationUi(FpFlow::Unlock, QStringLiteral("Preparing sensor…"));
    runExclusiveSerialOp([this]() {
        m_fpFlow = FpFlow::Unlock;
        m_fpUnlockMessage = QStringLiteral("Place your finger on the sensor");
        emit fingerprintUnlockActiveChanged();
        if (!m_serialBridge->tryStartVerify()) {
            abortFingerprintOperationUi();
            emit notificationReceived(
                QStringLiteral("Sensor is busy — please wait"), true);
            return;
        }
    });
}

void VaultController::beginSetupOwnerEnroll(int fingerId)
{
    Q_UNUSED(fingerId);
    if (!m_serialBridge->isConnected()) {
        tryConnectSerial();
        QTimer::singleShot(500, this, [this, fingerId]() {
            beginSetupOwnerEnrollAfterConnect(fingerId);
        });
        return;
    }
    if (!ensureFingerprintIdle())
        return;
    beginOwnerEnrollAuto();
}

void VaultController::clearSetupWizardSession()
{
    m_setupOwnerEnrolledThisSession = false;
    m_setupEnrollKeyGraceUntilMs = 0;
    m_pendingSetupGetKeyOnly = false;
}

void VaultController::startVaultSetupVerify()
{
    beginFingerprintOperationUi(FpFlow::SetupVerify,
                                QStringLiteral("Preparing sensor…"));
    runExclusiveSerialOp([this]() {
        if (!m_pendingSetup.valid)
            return;
        m_fpFlow = FpFlow::SetupVerify;
        m_fpUnlockMessage =
            QStringLiteral("Touch the sensor once to confirm setup");
        emit fingerprintUnlockActiveChanged();
        if (!m_serialBridge->tryStartVerify()) {
            m_pendingSetup.valid = false;
            abortFingerprintOperationUi();
            emit notificationReceived(
                QStringLiteral("Sensor is busy — please wait"), true);
        }
    });
}

void VaultController::requestVaultSetupKeyAfterEnroll()
{
    m_pendingSetupGetKeyOnly = true;
    beginFingerprintOperationUi(FpFlow::SetupVerify,
                                QStringLiteral("Starting vault setup…"));
    runExclusiveSerialOp([this]() {
        if (!m_pendingSetup.valid)
            return;
        m_fpFlow = FpFlow::SetupVerify;
        m_fpUnlockMessage = QStringLiteral("Starting vault setup…");
        emit fingerprintUnlockActiveChanged();
        m_serialBridge->requestKey();
    });

    QTimer::singleShot(6000, this, [this]() {
        if (!m_pendingSetupGetKeyOnly || !m_fpUnlockActive || !m_pendingSetup.valid)
            return;
        m_pendingSetupGetKeyOnly = false;
        ++m_serialExclusiveGeneration;
        if (m_serialBridge)
            m_serialBridge->forceIdle();
        abortFingerprintOperationUi();
        emit notificationReceived(
            QStringLiteral("Confirm your fingerprint once to start setup."), false);
        startVaultSetupVerify();
    });
}

bool VaultController::beginVaultSetupBiometric(const QString &device, bool force, int vaultSizeMB,
                                                 const QString &vaultName, const QString &normalName,
                                                 const QString &normalFs)
{
    if (m_setupOwnerFingerId < 1) {
        emit notificationReceived(
            QStringLiteral("Enroll the owner fingerprint before starting setup"), true);
        return false;
    }
    if (!m_serialBridge->isConnected()) {
        emit notificationReceived(QStringLiteral("Fingerprint sensor not connected"), true);
        return false;
    }
    if (!ensureFingerprintIdle())
        return false;

    m_pendingSetup.device = device;
    m_pendingSetup.force = force;
    m_pendingSetup.vaultSizeMB = vaultSizeMB;
    m_pendingSetup.vaultName = vaultName;
    m_pendingSetup.normalName = normalName;
    m_pendingSetup.normalFs = normalFs;
    m_pendingSetup.valid = true;

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const bool skipVerify =
        m_setupOwnerEnrolledThisSession && nowMs < m_setupEnrollKeyGraceUntilMs;

    if (skipVerify)
        requestVaultSetupKeyAfterEnroll();
    else
        startVaultSetupVerify();
    return true;
}

void VaultController::cancelFingerprintUnlock()
{
    ++m_serialExclusiveGeneration;
    m_pendingSetupGetKeyOnly = false;
    if (m_serialBridge)
        m_serialBridge->forceIdle();
    const bool grantEnrollOrphan = (m_fpFlow == FpFlow::GrantEnroll);
    if (grantEnrollOrphan)
        cleanupGrantEnrollOrphan();
    endFingerprintOperationUi();
}

void VaultController::finishUnlockWithKey(const QByteArray &key32)
{
    QByteArray key = key32;
    m_isToggling = true;
    emit togglingChanged();
    updateActivity(QStringLiteral("Kasa şifresi çözülüyor…"));

    const QString device = m_selectedDevice;
    const QString mount = m_mountPoint;
    QFuture<int> future = QtConcurrent::run([this, device, mount, key]() {
        return runVaultManagerWithKey(
            {QStringLiteral("open"), device, mount, QStringLiteral("--key-from-stdin")},
            key, 60000);
    });

    auto watcher = new QFutureWatcher<int>(this);
    connect(watcher, &QFutureWatcher<int>::finished, this, [this, watcher, key]() {
        const int code = watcher->result();
        QByteArray wiped = key;
        wiped.fill('\0');

        m_isToggling = false;
        emit togglingChanged();

        if (code == 0) {
            m_isLocked = false;
            m_unlockedDevice = m_selectedDevice;
            saveState(QStringLiteral("unlocked"), false);
            addLog(QStringLiteral("SUCCESS"), QStringLiteral("Vault unlocked via biometric"));
            updateActivity(QStringLiteral("Kasa açıldı"));
            emit notificationReceived(QStringLiteral("Kasa açıldı ve bağlandı"), false);
            resetAutoLockTimer();
            QProcess::startDetached(QStringLiteral("xdg-open"), {m_mountPoint});
        } else {
            emit notificationReceived(QStringLiteral("Kasa açılamadı"), true);
        }
        emit statusChanged();
        watcher->deleteLater();
    });
    watcher->setFuture(future);
}

void VaultController::beginUnlockVault()
{
    beginFingerprintUnlock();
}

void VaultController::beginLockVault()
{
    m_isToggling = true;
    emit togglingChanged();
    updateActivity("Locking vault...");
    emit statusChanged();

    QFuture<bool> future = QtConcurrent::run(&VaultController::unmountVault, this);
    auto watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher]() {
        const bool success = watcher->result();
        m_isToggling = false;
        emit togglingChanged();

        if (success) {
            m_isLocked = true;
            m_unlockedDevice.clear();
            saveState(QStringLiteral("locked"), false);
            updateActivity("Vault locked successfully.");
            emit notificationReceived("Vault locked and dismounted.", false);
        } else {
            emit notificationReceived("Failed to lock vault!", true);
            updateActivity("Lock failed.");
        }
        emit statusChanged();
        watcher->deleteLater();
    });
    watcher->setFuture(future);
}

void VaultController::toggleVault()
{
    if (m_isToggling) return;
    resetAutoLockTimer();

    if (m_selectedDevice.isEmpty()) {
        emit notificationReceived("Select a vault first.", true);
        return;
    }

    if (isVaultUnlockedForDevice(m_selectedDevice)) {
        beginLockVault();
        return;
    }

    if (isVaultRuntimeOpen() && !m_unlockedDevice.isEmpty()
        && m_unlockedDevice != m_selectedDevice) {
        const QString label = registryLabelForDevice(m_unlockedDevice);
        const QString who = label.isEmpty() ? m_unlockedDevice : label;
        emit notificationReceived(
            QStringLiteral("Another vault is open (%1). Lock it before opening this one.").arg(who),
            true);
        return;
    }

    if (m_serialBridge && m_serialBridge->isConnected()) {
        beginFingerprintUnlock();
        return;
    }
    beginUnlockVault();
}

/* ─── Vault Setup (First-Time LUKS Format) ───────────────────────────────── */

bool VaultController::setupVault(const QString &device, bool force, int ownerId, int vaultSizeMB,
                                 const QString &vaultName, const QString &normalName, const QString &normalFs)
{
    if (device.isEmpty()) return false;
    if (m_isSetupRunning) {
        emit notificationReceived("Setup is already in progress!", true);
        return false;
    }
    if (!device.startsWith("/dev/")) {
        emit notificationReceived("Setup requires a block device (/dev/...).", true);
        return false;
    }
    if (!simulatorRunning()) {
        emit notificationReceived("Start the simulator first!", true);
        return false;
    }
    if (!reconcileVaultRuntime()) {
        emit notificationReceived(
            QStringLiteral("System is busy — close apps using the vault and retry."), true);
        return false;
    }
    if (m_serialBridge && m_serialBridge->isConnected()) {
        m_setupOwnerFingerId = ownerId;
        return beginVaultSetupBiometric(device, force, vaultSizeMB, vaultName, normalName, normalFs);
    }
    launchSetupAsync(device, ownerId, force, vaultSizeMB, vaultName, normalName, normalFs, QByteArray());
    return true;
}

void VaultController::launchSetupAsync(const QString &device, int ownerId, bool force, int vaultSizeMB,
                                       const QString &vaultName, const QString &normalName,
                                       const QString &normalFs, const QByteArray &key32)
{
    m_isSetupRunning = true;
    m_setupStatusText = QStringLiteral("Formatting encrypted vault on ") + device + "...";
    saveState(QStringLiteral("locked"), true);
    emit setupRunningChanged();
    addLog("INFO", QString("Formatting new encrypted vault on: %1").arg(device));

    m_setupWatcher = new QFutureWatcher<int>(this);
    connect(m_setupWatcher, &QFutureWatcher<int>::finished, this,
            [this, device, vaultName, ownerId]() {
        const int ret = m_setupWatcher->result();
        m_isSetupRunning = false;
        m_setupStatusText.clear();
        emit setupRunningChanged();
        m_setupWatcher->deleteLater();
        m_setupWatcher = nullptr;
        saveState(QStringLiteral("locked"), false);
        reconcileVaultRuntime();

        if (ret == 0) {
            m_selectedDevice = device;
            QJsonObject regEntry = registryEntryForDevice(device);
            if (regEntry.isEmpty())
                regEntry.insert(QStringLiteral("path"), device);
            regEntry.insert(QStringLiteral("isVault"), true);
            if (!vaultName.isEmpty())
                regEntry.insert(QStringLiteral("vaultName"), vaultName);
            regEntry.insert(QStringLiteral("ownerFingerId"), ownerId);
            QJsonArray accessList;
            QJsonObject ownerEntry;
            ownerEntry.insert(QStringLiteral("fingerId"), ownerId);
            ownerEntry.insert(QStringLiteral("isOwner"), true);
            const QString ownerLabel = m_setupOwnerLabel.trimmed().isEmpty()
                                           ? (vaultName.isEmpty() ? QStringLiteral("Sahip")
                                                                  : vaultName)
                                           : m_setupOwnerLabel.trimmed();
            ownerEntry.insert(QStringLiteral("label"), ownerLabel);
            accessList.append(ownerEntry);
            regEntry.insert(QStringLiteral("accessList"), accessList);
            saveRegistryEntry(regEntry);
            addLog("SUCCESS", "Vault setup complete: " + device);
            refreshStorageDevices();
            selectDeviceByPath(device, true);
            emit setupFinished(true, QStringLiteral("Vault setup complete!"));
            emit notificationReceived(QStringLiteral("Vault initialized!"), false);
            emit navigationRequested(QStringLiteral("VaultManager.qml"));
        } else if (ret == 3) {
            emit setupFinished(false, QStringLiteral("Device already has a vault."));
            emit notificationReceived(QStringLiteral("Vault already exists."), false);
        } else if (ret == 4) {
            emit setupFinished(false, QStringLiteral("Existing data — enable Force format."));
            emit notificationReceived(QStringLiteral("Enable Force Format to override."), true);
        } else {
            if (!m_lastVaultManagerOutput.isEmpty())
                addLog(QStringLiteral("ERROR"), m_lastVaultManagerOutput);
            QString detail =
                QStringLiteral("unknown error (exit %1)").arg(ret);
            if (!m_lastVaultManagerOutput.isEmpty()) {
                for (const QString &line :
                     m_lastVaultManagerOutput.split(QLatin1Char('\n'))) {
                    const QString trimmed = line.trimmed();
                    if (!trimmed.isEmpty())
                        detail = trimmed;
                }
            }
            emit setupFinished(false, QStringLiteral("Setup failed."));
            emit notificationReceived(
                QStringLiteral("Vault setup failed: %1").arg(detail), true);
        }
    });

    QStringList args;
    args << QStringLiteral("setup") << device;
    if (key32.size() == 32)
        args << QStringLiteral("--key-from-stdin");
    else
        args << QString::number(ownerId);
    if (force) args << QStringLiteral("--force");
    if (vaultSizeMB > 0) args << QStringLiteral("--partition") << QString::number(vaultSizeMB);
    if (!vaultName.isEmpty()) args << QStringLiteral("--vault-name") << vaultName;
    if (!normalName.isEmpty()) args << QStringLiteral("--normal-name") << normalName;
    if (!normalFs.isEmpty()) args << QStringLiteral("--normal-fs") << normalFs;

    const QString bin = m_vaultManagerBin;
    const QByteArray keyCopy = key32;
    QFuture<int> future = QtConcurrent::run([this, bin, args, keyCopy]() -> int {
        if (keyCopy.size() == 32)
            return runVaultManagerWithKey(args, keyCopy, 120000);
        QProcess proc;
        proc.setProcessChannelMode(QProcess::MergedChannels);
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("SUDO_UID", QString::number(getuid()));
        env.insert("SUDO_GID", QString::number(getgid()));
        proc.setProcessEnvironment(env);
        proc.start(QStringLiteral("pkexec"), QStringList() << bin << args);
        if (!proc.waitForStarted(5000) || !proc.waitForFinished(120000)) {
            proc.kill();
            return -1;
        }
        return proc.exitCode();
    });
    m_setupWatcher->setFuture(future);
}

void VaultController::finishSetupWithKey(const QByteArray &key32)
{
    if (!m_pendingSetup.valid) {
        emit notificationReceived(QStringLiteral("Kurulum oturumu geçersiz"), true);
        return;
    }
    const int ownerId = m_setupOwnerFingerId;
    launchSetupAsync(m_pendingSetup.device, ownerId, m_pendingSetup.force, m_pendingSetup.vaultSizeMB,
                     m_pendingSetup.vaultName, m_pendingSetup.normalName, m_pendingSetup.normalFs,
                     key32);
    m_pendingSetup.valid = false;
}

/* ─── Fingerprint Operations (Real HAL via Socket) ───────────────────────── */

void VaultController::loadFingerprintsFromHAL()
{
    QJsonObject cmd;
    cmd["cmd"] = "list";

    QByteArray resp = sendSocketCommand(m_socketPath, cmd);
    if (resp.isEmpty()) {
        /* Simülatör çalışmıyor olabilir, sessizce çık */
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(resp);
    if (!doc.isObject()) return;

    QJsonObject obj = doc.object();
    if (obj["status"].toString() != "ok") return;

    QJsonArray fingers = obj["fingers"].toArray();
    m_fingerprints.clear();

    for (const auto &f : fingers) {
        QJsonObject fobj = f.toObject();
        QVariantMap fp;
        fp["label"] = fobj["name"].toString();
        fp["slot"]  = fobj["finger_id"].toInt();
        m_fingerprints << fp;
    }

    emit fingerprintsChanged();
}

void VaultController::refreshFingerprints()
{
    loadFingerprintsFromHAL();
    emit notificationReceived("Fingerprint list refreshed.", false);
}

void VaultController::addFingerprint()
{
    if (!simulatorRunning()) {
        emit notificationReceived("Start the simulator first!", true);
        return;
    }

    /* Boş slot bul (1-127 arası) */
    int nextId = 1;
    for (const auto &fp : m_fingerprints) {
        int slot = fp.toMap()["slot"].toInt();
        if (slot >= nextId) nextId = slot + 1;
    }
    if (nextId > 127) {
        emit notificationReceived("Fingerprint database is full!", true);
        return;
    }

    emit notificationReceived("Enrolling fingerprint #" + QString::number(nextId) + "...", false);

    /* Simülatöre enroll komutu gönder */
    QJsonObject cmd;
    cmd["cmd"] = "enroll";
    cmd["finger_id"] = nextId;
    cmd["name"] = "Fingerprint " + QString::number(nextId);

    QByteArray resp = sendSocketCommand(m_socketPath, cmd);
    if (resp.isEmpty()) {
        emit notificationReceived("Failed to communicate with simulator!", true);
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(resp);
    QJsonObject obj = doc.object();

    if (obj["status"].toString() == "ok") {
        loadFingerprintsFromHAL();
        QString msg = "Fingerprint #" + QString::number(nextId) + " registered!";
        addLog("SUCCESS", "New fingerprint enrolled (ID: " + QString::number(nextId) + ")");
        updateActivity(msg);
        emit statusChanged();
        emit notificationReceived(msg, false);
    } else {
        QString reason = obj["reason"].toString();
        emit notificationReceived("Enroll failed: " + reason, true);
    }
}

void VaultController::removeFingerprint(int index)
{
    if (index < 0 || index >= m_fingerprints.size()) return;

    if (!simulatorRunning()) {
        emit notificationReceived("Start the simulator first!", true);
        return;
    }

    int fingerId = m_fingerprints.at(index).toMap()["slot"].toInt();

    QJsonObject cmd;
    cmd["cmd"] = "delete";
    cmd["finger_id"] = fingerId;

    QByteArray resp = sendSocketCommand(m_socketPath, cmd);
    if (resp.isEmpty()) {
        emit notificationReceived("Failed to communicate with simulator!", true);
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(resp);
    QJsonObject obj = doc.object();

    if (obj["status"].toString() == "ok") {
        loadFingerprintsFromHAL();
        QString msg = "Fingerprint #" + QString::number(fingerId) + " removed.";
        addLog("WARNING", "Access revoked for Fingerprint ID: " + QString::number(fingerId));
        updateActivity(msg);
        emit statusChanged();
        emit notificationReceived(msg, false);
    } else {
        QString reason = obj["reason"].toString();
        emit notificationReceived("Delete failed: " + reason, true);
    }
}

QString VaultController::fingerprintLabel(int index) const
{
    if (index >= 0 && index < m_fingerprints.size()) {
        return m_fingerprints.at(index).toMap().value("label").toString();
    }
    return QString("Fingerprint #%1").arg(index + 1);
}

namespace {

struct ChildPartitionSummary {
    bool hasLuks = false;
    QString luksSize;
    QString normalSize;
};

ChildPartitionSummary summarizePartitions(const QJsonArray &children)
{
    ChildPartitionSummary summary;
    for (const QJsonValue &v : children) {
        const QJsonObject child = v.toObject();
        if (child.value(QStringLiteral("type")).toString() != QStringLiteral("part"))
            continue;

        const QString fstype = child.value(QStringLiteral("fstype")).toString();
        const QString childSize = child.value(QStringLiteral("size")).toString();
        if (fstype == QStringLiteral("crypto_LUKS")) {
            summary.hasLuks = true;
            summary.luksSize = childSize;
        } else {
            summary.normalSize = childSize;
        }
    }
    return summary;
}

void insertActionFields(QVariantMap &entry,
                        const QString &kind,
                        bool isVault,
                        bool canDelete,
                        const QString &deleteTargetPath = QString())
{
    entry.insert(QStringLiteral("kind"), kind);
    entry.insert(QStringLiteral("canDelete"), canDelete);
    entry.insert(QStringLiteral("deleteTargetPath"),
                 canDelete ? deleteTargetPath : QString());
    entry.insert(QStringLiteral("canSetup"), !isVault);
    entry.insert(QStringLiteral("canOpen"), isVault);
}

QVariantMap makeDigitalVaultLoopEntry(const QJsonObject &obj,
                                      const QString &devPath,
                                      const QString &label,
                                      const ChildPartitionSummary &summary)
{
    const QString name = obj.value(QStringLiteral("name")).toString();
    const QString size = obj.value(QStringLiteral("size")).toString();
    const bool isVault = summary.hasLuks;

    QVariantMap entry;
    entry.insert(QStringLiteral("path"), devPath);
    entry.insert(QStringLiteral("isVault"), isVault);

    if (isVault) {
        entry.insert(QStringLiteral("name"), label.isEmpty()
            ? QStringLiteral("Vault (%1)").arg(size)
            : QStringLiteral("%1 (%2)").arg(label, size));
        if (!summary.luksSize.isEmpty())
            entry.insert(QStringLiteral("luksSize"), summary.luksSize);
        QString description = QStringLiteral("Encrypted vault");
        if (!summary.normalSize.isEmpty())
            description += QStringLiteral(" — Normal storage (%1)").arg(summary.normalSize);
        entry.insert(QStringLiteral("description"), description);
    } else {
        entry.insert(QStringLiteral("name"), label.isEmpty()
            ? QStringLiteral("Loop: %1").arg(name)
            : QStringLiteral("%1 (%2)").arg(label, size));
        entry.insert(QStringLiteral("description"),
                     QStringLiteral("DigitalVault container"));
    }

    insertActionFields(entry, QStringLiteral("digitalVaultLoop"), isVault, true, devPath);
    return entry;
}

QVariantMap makeVaultPartitionEntry(const QString &devPath, const QString &size)
{
    QVariantMap entry;
    entry.insert(QStringLiteral("path"), devPath);
    entry.insert(QStringLiteral("name"), QStringLiteral("Vault: %1").arg(size));
    entry.insert(QStringLiteral("luksSize"), size);
    entry.insert(QStringLiteral("description"), QStringLiteral("Encrypted vault"));
    entry.insert(QStringLiteral("isVault"), true);
    insertActionFields(entry, QStringLiteral("vaultPartition"), true, false);
    return entry;
}

QVariantMap makeGenericStorageEntry(const QJsonObject &obj,
                                    const QString &devPath,
                                    const QString &type)
{
    const QString name = obj.value(QStringLiteral("name")).toString();
    const QString size = obj.value(QStringLiteral("size")).toString();

    QVariantMap entry;
    entry.insert(QStringLiteral("path"), devPath);
    entry.insert(QStringLiteral("isVault"), false);
    entry.insert(QStringLiteral("name"), QStringLiteral("/dev/%1 (%2)").arg(name, size));

    QString info = obj.value(QStringLiteral("model")).toString()
                   + QStringLiteral(" ")
                   + obj.value(QStringLiteral("vendor")).toString();
    info = info.trimmed();
    if (info.isEmpty())
        info = (type == QStringLiteral("disk")) ? QStringLiteral("Physical disk")
                                              : QStringLiteral("Storage partition");
    entry.insert(QStringLiteral("description"), info);

    const QString kind = (type == QStringLiteral("disk")) ? QStringLiteral("disk")
                                                        : QStringLiteral("partition");
    insertActionFields(entry, kind, false, false);
    return entry;
}

} // namespace

QVariantList VaultController::getStorageDevices()
{
    QVariantList devices;

    QProcess lsblk;
    lsblk.start(QStringLiteral("lsblk"),
                {QStringLiteral("-Jno"),
                 QStringLiteral("NAME,SIZE,TYPE,MOUNTPOINT,MODEL,VENDOR,FSTYPE")});
    if (!lsblk.waitForFinished(3000))
        return devices;

    const QJsonDocument doc = QJsonDocument::fromJson(lsblk.readAllStandardOutput());
    if (!doc.isObject())
        return devices;

    const QJsonArray blockDevices = doc.object()[QStringLiteral("blockdevices")].toArray();

    std::function<void(const QJsonArray &, const QString &)> parseDevices;
    parseDevices = [&](const QJsonArray &devs, const QString &parentPath) {
        for (const QJsonValue &dev : devs) {
            const QJsonObject obj = dev.toObject();
            const QString type = obj.value(QStringLiteral("type")).toString();
            const QString fstype = obj.value(QStringLiteral("fstype")).toString();
            const QString name = obj.value(QStringLiteral("name")).toString();
            const QString size = obj.value(QStringLiteral("size")).toString();
            const QString devPath = QStringLiteral("/dev/") + name;
            const bool isLuks = (fstype == QStringLiteral("crypto_LUKS"));
            const QJsonArray children = obj.value(QStringLiteral("children")).toArray();

            if (type == QStringLiteral("loop")) {
                if (obj.value(QStringLiteral("mountpoint")).toString().startsWith(QStringLiteral("/snap")))
                    continue;
                if (!isRegistryLoopDevice(devPath))
                    continue;

                const QString label = registryLabelForDevice(devPath);
                if (!children.isEmpty()) {
                    devices << makeDigitalVaultLoopEntry(obj, devPath, label,
                                                         summarizePartitions(children));
                    continue;
                }

                devices << makeDigitalVaultLoopEntry(obj, devPath, label, {});
                continue;
            }

            if (type == QStringLiteral("part")) {
                if (!parentPath.isEmpty() && isRegistryLoopDevice(parentPath))
                    continue;

                if (isLuks) {
                    devices << makeVaultPartitionEntry(devPath, size);
                    continue;
                }

                devices << makeGenericStorageEntry(obj, devPath, type);
                continue;
            }

            if (type == QStringLiteral("disk")) {
                QVariantMap entry = makeGenericStorageEntry(obj, devPath, type);
                if (!children.isEmpty()) {
                    const ChildPartitionSummary summary = summarizePartitions(children);
                    if (summary.hasLuks) {
                        QString desc = entry.value(QStringLiteral("description")).toString();
                        desc += QStringLiteral(" — Encrypted vault (%1)").arg(summary.luksSize);
                        if (!summary.normalSize.isEmpty())
                            desc += QStringLiteral(", normal storage (%1)").arg(summary.normalSize);
                        entry.insert(QStringLiteral("description"), desc);
                    }
                }
                devices << entry;

                if (!children.isEmpty()) {
                    const QString childParent = devPath;
                    parseDevices(children, childParent);
                }
                continue;
            }

            if (!children.isEmpty())
                parseDevices(children, parentPath);
        }
    };

    parseDevices(blockDevices, QString());
    return devices;
}

bool VaultController::deviceListsEqual(const QVariantList &a, const QVariantList &b)
{
    if (a.size() != b.size())
        return false;
    for (int i = 0; i < a.size(); ++i) {
        const QVariantMap ma = a.at(i).toMap();
        const QVariantMap mb = b.at(i).toMap();
        if (ma.value(QStringLiteral("path")).toString() != mb.value(QStringLiteral("path")).toString())
            return false;
        if (ma.value(QStringLiteral("isVault")).toBool() != mb.value(QStringLiteral("isVault")).toBool())
            return false;
    }
    return true;
}

void VaultController::refreshStorageDevices()
{
    const QVariantList next = getStorageDevices();
    if (deviceListsEqual(next, m_storageDevices))
        return;
    m_storageDevices = next;
    emit devicesChanged();
}

/* ─── Periodic Status Poll ───────────────────────────────────────────────── */

void VaultController::pollStatus()
{
    /*
     * LUKS durumunu kontrol et:
     * /dev/mapper/digitalvault varsa vault açık demektir.
     */
    bool wasLocked = m_isLocked;

    /* 
     * HAL Bağlantısını Kontrol Et (Simülatör ya da Gerçek Donanım)
     * Eğer HAL koptuysa ve vault açıksa, acil durum kilitlemesi yap.
     */
    if (!simulatorRunning()) {
        if (isVaultRuntimeOpen() && !m_isToggling && !s_emergencyClosing.load()) {
            addLog("CRITICAL", "HAL disconnected — emergency locking vault.");
            emergencyClose();
        }
    }

    if (!m_isLocked && !isVaultRuntimeOpen()) {
        m_isLocked = true;
        m_unlockedDevice.clear();
        saveState(QStringLiteral("locked"), false);
        addLog("WARNING", "Vault was closed externally");
    }

    bool stateChanged = (wasLocked != m_isLocked);
    if (!isVaultRuntimeOpen() && !m_unlockedDevice.isEmpty()) {
        m_unlockedDevice.clear();
        stateChanged = true;
    }
    if (stateChanged)
        emit statusChanged();

    refreshStorageDevices();
}

QString VaultController::defaultContainersDir() const
{
    return m_containersDir;
}

QString VaultController::sanitizeContainerName(const QString &name) const
{
    QString s = name.trimmed().toLower();
    s.replace(QRegularExpression(QStringLiteral("[^a-z0-9_-]")), QStringLiteral("_"));
    s.replace(QRegularExpression(QStringLiteral("_+")), QStringLiteral("_"));
    if (s.isEmpty()) s = QStringLiteral("vault");
    return s.left(48);
}

QJsonArray VaultController::loadRegistryArray() const
{
    QFile f(m_registryPath);
    if (!f.open(QIODevice::ReadOnly))
        return QJsonArray();
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray())
        return QJsonArray();
    return doc.array();
}

bool VaultController::saveRegistryEntry(const QJsonObject &entry)
{
    QJsonArray arr = loadRegistryArray();
    const QString backing = entry.value(QStringLiteral("backing")).toString();
    const QString path = entry.value(QStringLiteral("path")).toString();
    QJsonArray updated;
    bool replaced = false;
    for (const QJsonValue &v : arr) {
        QJsonObject o = v.toObject();
        if ((!backing.isEmpty() && o.value(QStringLiteral("backing")).toString() == backing)
            || (!path.isEmpty() && o.value(QStringLiteral("path")).toString() == path)) {
            updated.append(entry);
            replaced = true;
        } else {
            updated.append(o);
        }
    }
    if (!replaced)
        updated.append(entry);

    QFile f(m_registryPath);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(updated).toJson(QJsonDocument::Indented));
    return true;
}

void VaultController::syncRegistryWithLosetup()
{
    QProcess losetup;
    losetup.start(QStringLiteral("losetup"), {QStringLiteral("-J")});
    if (!losetup.waitForFinished(3000))
        return;

    QJsonDocument doc = QJsonDocument::fromJson(losetup.readAllStandardOutput());
    if (!doc.isObject())
        return;

    QHash<QString, QString> backingToLoop;
    const QJsonArray loops = doc.object().value(QStringLiteral("loopdevices")).toArray();
    for (const QJsonValue &v : loops) {
        const QJsonObject o = v.toObject();
        const QString backing = o.value(QStringLiteral("back-file")).toString();
        const QString dev = QStringLiteral("/dev/") + o.value(QStringLiteral("name")).toString();
        if (!backing.isEmpty() && dev.startsWith(QStringLiteral("/dev/loop")))
            backingToLoop.insert(backing, dev);
    }

    QJsonArray arr = loadRegistryArray();
    bool changed = false;
    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject o = arr.at(i).toObject();
        const QString backing = o.value(QStringLiteral("backing")).toString();
        if (backing.isEmpty())
            continue;
        const QString loop = backingToLoop.value(backing);
        if (!loop.isEmpty() && o.value(QStringLiteral("path")).toString() != loop) {
            o.insert(QStringLiteral("path"), loop);
            arr.replace(i, o);
            changed = true;
        }
    }
    if (!changed)
        return;

    QFile f(m_registryPath);
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

bool VaultController::isRegistryLoopDevice(const QString &devPath) const
{
    for (const QJsonValue &v : loadRegistryArray()) {
        if (v.toObject().value(QStringLiteral("path")).toString() == devPath)
            return true;
    }
    return false;
}

QString VaultController::registryLabelForDevice(const QString &devPath) const
{
    const QJsonObject entry = registryEntryForDevice(devPath);
    return entry.value(QStringLiteral("label")).toString();
}

QJsonObject VaultController::registryEntryForDevice(const QString &devPath) const
{
    for (const QJsonValue &v : loadRegistryArray()) {
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("path")).toString() == devPath)
            return o;
    }
    return {};
}

namespace {

constexpr int kMinVaultPartitionMB = 16;
constexpr int kMinNormalPartitionMB = 16;
constexpr int kDefaultVaultSizeCapMB = 2048;

} // namespace

QVariantMap VaultController::resolveDeviceInfo(const QString &path) const
{
    QVariantMap info;
    info.insert(QStringLiteral("valid"), false);
    info.insert(QStringLiteral("path"), path);

    if (!path.startsWith(QStringLiteral("/dev/"))) {
        info.insert(QStringLiteral("error"),
                    QStringLiteral("Not a block device path."));
        return info;
    }

    QString fstype;
    QString devType;
    int capacityMB = 0;

    QJsonObject lsblkRoot;
    QProcess lsblk;
    lsblk.start(QStringLiteral("lsblk"),
                {QStringLiteral("-Jbno"), QStringLiteral("FSTYPE,SIZE,TYPE"), path});
    if (lsblk.waitForFinished(2000)) {
        const QJsonDocument doc = QJsonDocument::fromJson(lsblk.readAllStandardOutput());
        if (doc.isObject()) {
            const QJsonArray devs = doc.object()[QStringLiteral("blockdevices")].toArray();
            if (!devs.isEmpty()) {
                lsblkRoot = devs.at(0).toObject();
                fstype = lsblkRoot[QStringLiteral("fstype")].toString();
                devType = lsblkRoot[QStringLiteral("type")].toString();
                const qlonglong bytes = lsblkRoot[QStringLiteral("size")].toVariant().toLongLong();
                capacityMB = static_cast<int>(bytes / (1024 * 1024));
            }
        }
    }

    const QJsonObject reg = registryEntryForDevice(path);
    const bool isRegistryLoop = !reg.isEmpty();
    QString label = reg.value(QStringLiteral("label")).toString();
    const QString backing = reg.value(QStringLiteral("backing")).toString();
    const int registrySizeMB = reg.value(QStringLiteral("sizeMB")).toInt();
    bool isVault = (fstype == QStringLiteral("crypto_LUKS"));
    if (reg.value(QStringLiteral("isVault")).toBool())
        isVault = true;
    if (!isVault && lsblkRoot.contains(QStringLiteral("children"))) {
        const QJsonArray children = lsblkRoot[QStringLiteral("children")].toArray();
        for (const QJsonValue &v : children) {
            if (v.toObject().value(QStringLiteral("fstype")).toString()
                == QStringLiteral("crypto_LUKS")) {
                isVault = true;
                break;
            }
        }
    }

    if (capacityMB <= 0 && registrySizeMB > 0)
        capacityMB = registrySizeMB;

    if (devType.isEmpty()) {
        if (path.startsWith(QStringLiteral("/dev/loop")))
            devType = QStringLiteral("loop");
        else
            devType = QStringLiteral("disk");
    }

    const bool isSetupCapable = capacityMB >= kMinVaultPartitionMB;

    info.insert(QStringLiteral("valid"), isSetupCapable);
    info.insert(QStringLiteral("capacityMB"), capacityMB);
    info.insert(QStringLiteral("fstype"), fstype);
    info.insert(QStringLiteral("type"), devType);
    info.insert(QStringLiteral("label"), label);
    info.insert(QStringLiteral("backingPath"), backing);
    info.insert(QStringLiteral("isVault"), isVault);
    info.insert(QStringLiteral("isRegistryLoop"), isRegistryLoop);
    info.insert(QStringLiteral("isSetupCapable"), isSetupCapable);
    if (!isSetupCapable) {
        info.insert(QStringLiteral("error"),
                    QStringLiteral("Device has no known capacity or is too small."));
    }

    return info;
}

void VaultController::applySelectedDeviceInfo(const QVariantMap &info)
{
    m_selectedDeviceInfo = info;
    m_selectedDevice = info.value(QStringLiteral("path")).toString();
    m_selectedDeviceCapacityMB = info.value(QStringLiteral("capacityMB")).toInt();
    m_isSelectedDeviceVault = info.value(QStringLiteral("isVault")).toBool();
}

void VaultController::selectDeviceByPath(const QString &path, bool forceRefresh)
{
    if (path.isEmpty())
        return;
    if (!forceRefresh && m_selectedDevice == path)
        return;

    applySelectedDeviceInfo(resolveDeviceInfo(path));
    emit statusChanged();
}

QVariantMap VaultController::createSetupSession(const QString &devicePath) const
{
    QVariantMap session;
    session.insert(QStringLiteral("valid"), false);

    const QString path = devicePath.trimmed().isEmpty() ? m_selectedDevice : devicePath.trimmed();
    const QVariantMap device = resolveDeviceInfo(path);

    if (!device.value(QStringLiteral("valid")).toBool()) {
        session.insert(QStringLiteral("error"),
                       device.value(QStringLiteral("error")).toString().isEmpty()
                           ? QStringLiteral("Select a block device with known capacity first.")
                           : device.value(QStringLiteral("error")).toString());
        return session;
    }

    const int capacityMB = device.value(QStringLiteral("capacityMB")).toInt();
    const int maxVaultMB = qMax(kMinVaultPartitionMB, capacityMB - kMinNormalPartitionMB);
    const int defaultVaultMB = qBound(kMinVaultPartitionMB,
                                      qMin(kDefaultVaultSizeCapMB, capacityMB / 2),
                                      maxVaultMB);
    const bool canCreateNormalPartition =
        (capacityMB - defaultVaultMB) >= kMinNormalPartitionMB;
    const bool isVault = device.value(QStringLiteral("isVault")).toBool();

    session.insert(QStringLiteral("valid"), true);
    session.insert(QStringLiteral("devicePath"), path);
    session.insert(QStringLiteral("capacityMB"), capacityMB);
    session.insert(QStringLiteral("minVaultMB"), kMinVaultPartitionMB);
    session.insert(QStringLiteral("minNormalMB"), kMinNormalPartitionMB);
    session.insert(QStringLiteral("maxVaultMB"), maxVaultMB);
    session.insert(QStringLiteral("defaultVaultMB"), defaultVaultMB);
    session.insert(QStringLiteral("canCreateNormalPartition"), canCreateNormalPartition);
    session.insert(QStringLiteral("requiresForceConfirmation"), isVault);
    session.insert(QStringLiteral("isVault"), isVault);
    session.insert(QStringLiteral("isRegistryLoop"),
                   device.value(QStringLiteral("isRegistryLoop")));
    session.insert(QStringLiteral("defaultVaultName"), QStringLiteral("My Vault"));
    session.insert(QStringLiteral("defaultNormalName"), QStringLiteral("Normal"));
    session.insert(QStringLiteral("proposedOwnerSlot"), ownerFingerSlotForDevice(path));
    session.insert(QStringLiteral("hasRegisteredOwner"), ownerFingerSlotForDevice(path) >= 1);

    return session;
}

void VaultController::prepareSetupOwnerSlot(const QString &devicePath)
{
    const QString path = devicePath.trimmed();
    m_setupDevicePath = path;
    const int regOwner = ownerFingerSlotForDevice(path);
    m_setupOwnerFingerId = regOwner >= 1 ? regOwner : -1;
    emit setupOwnerFingerIdChanged();
}

bool VaultController::createLoopDevice(const QString &containerName, int totalSizeMB,
                                       const QString &storageDir)
{
    if (m_isSetupRunning) {
        emit notificationReceived("Another operation is already in progress!", true);
        return false;
    }

    const QString label = sanitizeContainerName(containerName);
    if (totalSizeMB < 16) {
        emit notificationReceived("Minimum container size is 16 MB.", true);
        return false;
    }

    QString dir = storageDir.trimmed();
    if (dir.isEmpty())
        dir = m_containersDir;
    QDir().mkpath(dir);

    const QString imagePath = QDir(dir).absoluteFilePath(label + QStringLiteral(".img"));
    if (QFile::exists(imagePath)) {
        emit notificationReceived("Container image already exists: " + imagePath, true);
        return false;
    }

    m_isSetupRunning = true;
    m_setupStatusText = QStringLiteral("Creating loop device for ") + label + QStringLiteral("...");
    emit setupRunningChanged();
    addLog("INFO", QStringLiteral("Creating container: ") + imagePath);

    struct CreateLoopContext {
        QString loopPath;
        QString error;
    };
    auto ctx = std::make_shared<CreateLoopContext>();

    const QString vaultManagerBin = m_vaultManagerBin;
    const int sizeMb = totalSizeMB;

    m_setupWatcher = new QFutureWatcher<int>(this);
    connect(m_setupWatcher, &QFutureWatcher<int>::finished, this,
            [this, ctx, imagePath, label, sizeMb]() {
        const int code = m_setupWatcher->result();
        m_isSetupRunning = false;
        m_setupStatusText.clear();
        emit setupRunningChanged();
        m_setupWatcher->deleteLater();
        m_setupWatcher = nullptr;

        if (code != 0 || ctx->loopPath.isEmpty()) {
            const QString err = ctx->error.isEmpty()
                ? QStringLiteral("Failed to create loop device.")
                : ctx->error;
            addLog("ERROR", err);
            emit loopCreateFinished(false, err);
            emit notificationReceived(err, true);
            return;
        }

        QJsonObject entry;
        entry.insert(QStringLiteral("backing"), imagePath);
        entry.insert(QStringLiteral("path"), ctx->loopPath);
        entry.insert(QStringLiteral("label"), label);
        entry.insert(QStringLiteral("sizeMB"), sizeMb);
        entry.insert(QStringLiteral("isVault"), false);
        saveRegistryEntry(entry);

        addLog("SUCCESS", QStringLiteral("Loop device ready: ") + ctx->loopPath
               + QStringLiteral(" (") + imagePath + QStringLiteral(")"));
        refreshStorageDevices();
        selectDeviceByPath(ctx->loopPath, true);
        const QString msg = QStringLiteral("Loop ready at ") + ctx->loopPath
            + QStringLiteral(" — run Setup Vault to encrypt.");
        emit loopCreateFinished(true, msg);
        emit notificationReceived(msg, false);
    });

    QFuture<int> future = QtConcurrent::run([vaultManagerBin, imagePath, sizeMb, ctx]() -> int {
        QProcess proc;
        proc.setProcessChannelMode(QProcess::MergedChannels);
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("SUDO_UID", QString::number(::getuid()));
        env.insert("SUDO_GID", QString::number(::getgid()));
        proc.setProcessEnvironment(env);

        QStringList launchArgs;
        launchArgs << vaultManagerBin
                   << QStringLiteral("createloop") << imagePath << QString::number(sizeMb);
        proc.start(QStringLiteral("pkexec"), launchArgs);
        if (!proc.waitForStarted(5000)) {
            ctx->error = QStringLiteral("Could not start vault_manager (pkexec).");
            return -1;
        }
        if (!proc.waitForFinished(120000)) {
            proc.kill();
            ctx->error = QStringLiteral("Create loop timed out.");
            return -1;
        }
        const int exitCode = proc.exitCode();
        const QString output = QString::fromUtf8(proc.readAll()).trimmed();
        for (const QString &line : output.split('\n')) {
            const QString t = line.trimmed();
            if (t.startsWith(QStringLiteral("/dev/loop"))) {
                ctx->loopPath = t;
                break;
            }
        }
        if (exitCode == 2)
            ctx->error = QStringLiteral("Container image already exists.");
        else if (ctx->loopPath.isEmpty())
            ctx->error = output.isEmpty() ? QStringLiteral("createloop failed.") : output;
        return (exitCode == 0 && !ctx->loopPath.isEmpty()) ? 0 : exitCode;
    });

    m_setupWatcher->setFuture(future);
    return true;
}

QString VaultController::registryBackingForDevice(const QString &devicePath) const
{
    for (const QJsonValue &v : loadRegistryArray()) {
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("path")).toString() == devicePath)
            return o.value(QStringLiteral("backing")).toString();
    }
    return QString();
}

bool VaultController::removeRegistryEntry(const QString &devicePath, QString *removedBacking)
{
    const QJsonArray arr = loadRegistryArray();
    QJsonArray out;
    bool found = false;
    QString backing;

    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("path")).toString() == devicePath) {
            found = true;
            backing = o.value(QStringLiteral("backing")).toString();
            continue;
        }
        out.append(v);
    }
    if (!found)
        return false;

    if (removedBacking)
        *removedBacking = backing;

    QFile f(m_registryPath);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(out).toJson(QJsonDocument::Indented));
    return true;
}

bool VaultController::deleteDevice(const QString &devicePath)
{
    return deleteVault(devicePath);
}

bool VaultController::deleteVault(const QString &devicePath)
{
    if (m_isSetupRunning) {
        emit notificationReceived("Another operation is already in progress!", true);
        return false;
    }

    const QString path = devicePath.trimmed();
    if (path.isEmpty()) {
        emit notificationReceived("No device selected.", true);
        return false;
    }

    if (!reconcileVaultRuntime()) {
        emit notificationReceived(
            QStringLiteral("System is busy — could not prepare for delete. Close apps using the vault and retry."),
            true);
        return false;
    }

    if (isVaultRuntimeOpen()) {
        addLog("INFO", QStringLiteral("Locking vault before delete..."));
        if (!emergencyClose()) {
            emit notificationReceived(
                "Could not lock the vault before delete. Close apps using the mount point.", true);
            return false;
        }
    }

    const QJsonObject reg = registryEntryForDevice(path);
    const bool isLoop = !reg.value(QStringLiteral("backing")).toString().isEmpty()
                        || (path.startsWith(QStringLiteral("/dev/loop")) && !reg.isEmpty());

    if (isLoop) {
        const QString backing = reg.value(QStringLiteral("backing")).toString();
        if (backing.isEmpty()) {
            emit notificationReceived("No backing image path in registry for this device.", true);
            return false;
        }

        m_isSetupRunning = true;
        m_setupStatusText = QStringLiteral("Deleting vault on ") + path + QStringLiteral("...");
        emit setupRunningChanged();
        addLog("INFO", QStringLiteral("Deleting loop vault: ") + path);

        struct DeleteContext {
            QString deletedBacking;
            QString error;
        };
        auto ctx = std::make_shared<DeleteContext>();

        const QString vaultManagerBin = m_vaultManagerBin;
        const QString backingPath = backing;

        m_setupWatcher = new QFutureWatcher<int>(this);
        connect(m_setupWatcher, &QFutureWatcher<int>::finished, this,
                [this, ctx, path, backing]() {
            const int code = m_setupWatcher->result();
            m_isSetupRunning = false;
            m_setupStatusText.clear();
            emit setupRunningChanged();
            m_setupWatcher->deleteLater();
            m_setupWatcher = nullptr;
            reconcileVaultRuntime();

            if (code != 0 || ctx->deletedBacking.isEmpty()) {
                const QString err = ctx->error.isEmpty()
                    ? QStringLiteral("Failed to delete vault.")
                    : ctx->error;
                addLog("ERROR", err);
                emit deviceDeleteFinished(false, err);
                emit notificationReceived(err, true);
                return;
            }

            removeRegistryEntry(path, nullptr);

            if (m_selectedDevice == path) {
                m_selectedDevice.clear();
                m_selectedDeviceCapacityMB = 0;
                m_isSelectedDeviceVault = false;
                emit statusChanged();
            }
            if (m_unlockedDevice == path)
                m_unlockedDevice.clear();

            refreshStorageDevices();
            addLog("SUCCESS", QStringLiteral("Deleted vault: ") + path);
            const QString msg = QStringLiteral("Vault deleted: ") + path;
            emit deviceDeleteFinished(true, msg);
            emit notificationReceived(msg, false);
        });

        const QString mountPoint = m_mountPoint;
        QFuture<int> future = QtConcurrent::run([vaultManagerBin, mountPoint, path, backingPath, ctx]() -> int {
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert("SUDO_UID", QString::number(::getuid()));
            env.insert("SUDO_GID", QString::number(::getgid()));

            auto runPk = [&](const QStringList &args) -> int {
                QProcess proc;
                proc.setProcessChannelMode(QProcess::MergedChannels);
                proc.setProcessEnvironment(env);
                proc.start(QStringLiteral("pkexec"), args);
                if (!proc.waitForFinished(120000)) {
                    proc.kill();
                    return -1;
                }
                return proc.exitCode();
            };

            runPk({vaultManagerBin, QStringLiteral("reconcile"), mountPoint});

            QProcess proc;
            proc.setProcessChannelMode(QProcess::MergedChannels);
            proc.setProcessEnvironment(env);
            QStringList launchArgs;
            launchArgs << vaultManagerBin << QStringLiteral("destroyloop") << path << backingPath;
            proc.start(QStringLiteral("pkexec"), launchArgs);
            if (!proc.waitForFinished(60000)) {
                proc.kill();
                ctx->error = QStringLiteral("destroyloop timed out.");
                return -1;
            }

            const QByteArray output = proc.readAllStandardOutput();
            const int exitCode = proc.exitCode();

            for (const QByteArray &line : output.split('\n')) {
                const QByteArray trimmed = line.trimmed();
                if (trimmed.startsWith("DELETED:")) {
                    ctx->deletedBacking = QString::fromUtf8(trimmed.mid(8));
                    break;
                }
            }

            runPk({vaultManagerBin, QStringLiteral("reconcile"), mountPoint});

            if (exitCode != 0 || ctx->deletedBacking.isEmpty()) {
                ctx->error = QString::fromUtf8(output).trimmed();
                if (ctx->error.isEmpty())
                    ctx->error = QStringLiteral("destroyloop failed (exit %1).").arg(exitCode);
                return exitCode != 0 ? exitCode : 1;
            }
            return 0;
        });

        m_setupWatcher->setFuture(future);
        return true;
    }

    const QString luksPart = findLuksPartitionPath(path);
    if (luksPart.isEmpty()) {
        emit notificationReceived("No encrypted vault partition found on this device.", true);
        return false;
    }

    m_isSetupRunning = true;
    m_setupStatusText = QStringLiteral("Deleting encrypted vault on ") + luksPart + QStringLiteral("...");
    emit setupRunningChanged();
    addLog("INFO", QStringLiteral("Deleting physical vault partition: ") + luksPart);

    const QString vaultManagerBin = m_vaultManagerBin;
    const QString registryPath = path;
    const QString mountPoint = m_mountPoint;

    m_setupWatcher = new QFutureWatcher<int>(this);
    connect(m_setupWatcher, &QFutureWatcher<int>::finished, this,
            [this, registryPath, luksPart]() {
        const int code = m_setupWatcher->result();
        m_isSetupRunning = false;
        m_setupStatusText.clear();
        emit setupRunningChanged();
        m_setupWatcher->deleteLater();
        m_setupWatcher = nullptr;
        reconcileVaultRuntime();

        if (code != 0) {
            const QString err = QStringLiteral("Failed to delete encrypted vault (exit %1).").arg(code);
            addLog("ERROR", err);
            emit deviceDeleteFinished(false, err);
            emit notificationReceived(err, true);
            return;
        }

        QJsonObject reg = registryEntryForDevice(registryPath);
        if (!reg.isEmpty()) {
            reg.remove(QStringLiteral("isVault"));
            reg.remove(QStringLiteral("vaultName"));
            reg.remove(QStringLiteral("ownerFingerId"));
            if (reg.value(QStringLiteral("backing")).toString().isEmpty())
                removeRegistryEntry(registryPath, nullptr);
            else
                saveRegistryEntry(reg);
        }

        if (m_selectedDevice == registryPath || m_selectedDevice == luksPart) {
            m_selectedDevice.clear();
            m_selectedDeviceCapacityMB = 0;
            m_isSelectedDeviceVault = false;
            emit statusChanged();
        }
        if (m_unlockedDevice == registryPath || m_unlockedDevice == luksPart)
            m_unlockedDevice.clear();

        refreshStorageDevices();
        const QString msg = QStringLiteral("Vault deleted from ") + luksPart;
        addLog("SUCCESS", msg);
        emit deviceDeleteFinished(true, msg);
        emit notificationReceived(msg, false);
    });

    QFuture<int> future = QtConcurrent::run([vaultManagerBin, mountPoint, luksPart]() -> int {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("SUDO_UID", QString::number(::getuid()));
        env.insert("SUDO_GID", QString::number(::getgid()));

        auto runPk = [&](const QStringList &args) -> int {
            QProcess proc;
            proc.setProcessChannelMode(QProcess::MergedChannels);
            proc.setProcessEnvironment(env);
            proc.start(QStringLiteral("pkexec"), args);
            if (!proc.waitForFinished(120000)) {
                proc.kill();
                return -1;
            }
            return proc.exitCode();
        };

        runPk({vaultManagerBin, QStringLiteral("reconcile"), mountPoint});
        const int delCode = runPk({vaultManagerBin, QStringLiteral("deletevault"), luksPart});
        runPk({vaultManagerBin, QStringLiteral("reconcile"), mountPoint});
        return delCode;
    });

    m_setupWatcher->setFuture(future);
    return true;
}

void VaultController::setSelectedDevice(const QString &path)
{
    selectDeviceByPath(path, false);
}

void VaultController::addLog(const QString &level, const QString &msg)
{
    QJsonObject log;
    log["time"] = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    log["level"] = level;
    log["message"] = msg;

    // Bellekteki listeye ekle (Dashboard için)
    m_securityLogs.prepend(log);
    if (m_securityLogs.size() > 50) m_securityLogs.removeLast();
    emit logsChanged();

    // Dosyaya yaz
    QFile file(m_logFilePath);
    if (file.open(QIODevice::ReadWrite)) {
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonArray array = doc.array();
        array.prepend(log); // En yeni en başa
        
        // Çok fazla log birikmesin (opsiyonel limit: 1000)
        while (array.size() > 1000) array.removeLast();

        file.seek(0);
        file.write(QJsonDocument(array).toJson());
        file.resize(file.pos());
        file.close();
    }
}

QVariantList VaultController::getSecurityLogsPage(int page, int pageSize)
{
    QFile file(m_logFilePath);
    if (!file.open(QIODevice::ReadOnly)) return {};

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonArray array = doc.array();
    file.close();

    QVariantList result;
    int start = page * pageSize;
    int end = qMin(start + pageSize, (int)array.size());

    for (int i = start; i < end; ++i) {
        result.append(array.at(i).toVariant());
    }
    return result;
}

void VaultController::notifyUser(const QString &message, bool isError)
{
    emit notificationReceived(message, isError);
}

void VaultController::navigateToPage(const QString &pageFile)
{
    emit navigationRequested(pageFile);
}

void VaultController::clearLogs()
{
    QFile file(m_logFilePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write("[]");
        file.close();
    }
    m_securityLogs.clear();
    emit logsChanged();
}

void VaultController::renameFingerprint(int index, const QString &newName)
{
    if (index < 0 || index >= m_fingerprints.size()) return;
    if (newName.isEmpty()) return;

    if (!simulatorRunning()) {
        emit notificationReceived("Start the simulator first!", true);
        return;
    }

    int fingerId = m_fingerprints.at(index).toMap()["slot"].toInt();
    QString oldName = m_fingerprints.at(index).toMap()["name"].toString();

    QJsonObject cmd;
    cmd["cmd"] = "rename";
    cmd["finger_id"] = fingerId;
    cmd["new_name"] = newName;

    QByteArray resp = sendSocketCommand(m_socketPath, cmd);
    if (resp.isEmpty()) {
        emit notificationReceived("Failed to communicate with simulator!", true);
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(resp);
    if (doc.object()["status"].toString() == "ok") {
        loadFingerprintsFromHAL();
        addLog("INFO", "Renamed Fingerprint ID " + QString::number(fingerId) + " from '" + oldName + "' to '" + newName + "'");
        emit statusChanged();
        emit notificationReceived("Fingerprint renamed!", false);
    } else {
        emit notificationReceived("Rename failed!", true);
    }
}

/* ─── Quick Access: File Manager & Terminal ──────────────────────────────── */

void VaultController::openInFileManager()
{
    if (!isVaultUnlockedForDevice(m_selectedDevice)) {
        emit notificationReceived("Unlock this vault first!", true);
        return;
    }

    QProcess::startDetached("xdg-open", {m_mountPoint});
    resetAutoLockTimer();
    addLog("INFO", "Opened vault in file manager");
}

void VaultController::openInTerminal()
{
    if (!isVaultUnlockedForDevice(m_selectedDevice)) {
        emit notificationReceived("Unlock this vault first!", true);
        return;
    }
    resetAutoLockTimer();

    /*
     * Yaygın terminal emülatörlerini dene:
     *   1. gnome-terminal (GNOME/Ubuntu)
     *   2. konsole (KDE)
     *   3. xfce4-terminal (XFCE)
     *   4. xterm (evrensel fallback)
     */
    struct { const char *cmd; QStringList args; } terminals[] = {
        { "gnome-terminal", {"--working-directory=" + m_mountPoint} },
        { "konsole",        {"--workdir", m_mountPoint} },
        { "xfce4-terminal", {"--working-directory=" + m_mountPoint} },
        { "xterm",          {"-e", "cd " + m_mountPoint + " && bash"} },
    };

    for (const auto &t : terminals) {
        QProcess which;
        which.start("which", QStringList() << t.cmd);
        which.waitForFinished(1000);
        if (which.exitCode() == 0) {
            QProcess::startDetached(t.cmd, t.args);
            addLog("INFO", "Opened terminal at " + m_mountPoint);
            return;
        }
    }

    emit notificationReceived("No supported terminal emulator found!", true);
}

/* ─── Multi-user Access Management ───────────────────────────────────── */

bool VaultController::verifyOwner(int fingerId)
{
    if (!simulatorRunning()) {
        emit notificationReceived(QStringLiteral("Simülatörü başlatın"), true);
        return false;
    }

    if (m_serialBridge && m_serialBridge->isConnected()) {
        if (m_serialBridge->isBusy()) {
            emit notificationReceived(QStringLiteral("Sensör meşgul"), true);
            return false;
        }
        QEventLoop loop;
        bool ok = false;
        int matched = -1;
        QMetaObject::Connection c = connect(
            m_serialBridge, &SerialBridge::verifyCompleted,
            &loop, [&](bool success, int mid, const QString &) {
                ok = success;
                matched = mid;
                loop.quit();
            });
        m_serialBridge->startVerify();
        QTimer::singleShot(16000, &loop, &QEventLoop::quit);
        loop.exec();
        QObject::disconnect(c);
        if (ok && matched == fingerId) {
            addLog(QStringLiteral("SUCCESS"),
                   QStringLiteral("Owner verified (ID %1)").arg(fingerId));
            return true;
        }
        emit notificationReceived(QStringLiteral("Sahip doğrulaması başarısız"), true);
        return false;
    }

    QJsonObject cmd;
    cmd["cmd"] = QStringLiteral("verify");
    cmd["finger_id"] = fingerId;
    QByteArray resp = sendSocketCommand(m_socketPath, cmd);
    if (resp.isEmpty()) {
        emit notificationReceived(QStringLiteral("Sensör iletişimi başarısız"), true);
        return false;
    }
    QJsonDocument doc = QJsonDocument::fromJson(resp);
    if (doc.object()["status"].toString() == "ok") {
        addLog(QStringLiteral("SUCCESS"), QStringLiteral("Owner verified (ID %1)").arg(fingerId));
        return true;
    }
    emit notificationReceived(QStringLiteral("Doğrulama başarısız"), true);
    return false;
}

void VaultController::beginGrantAccessEnroll(int ownerFingerId, int newFingerId, const QString &label)
{
    Q_UNUSED(newFingerId);
    if (m_selectedDevice.isEmpty()) {
        emit notificationReceived(QStringLiteral("No device selected"), true);
        return;
    }
    beginGrantAccessForVault(m_selectedDevice, label);
    Q_UNUSED(ownerFingerId);
}

bool VaultController::grantAccess(int ownerFingerId, int newFingerId)
{
    if (m_selectedDevice.isEmpty()) {
        emit notificationReceived(QStringLiteral("Cihaz seçilmedi"), true);
        return false;
    }
    if (!verifyOwner(ownerFingerId)) {
        return false;
    }

    QJsonObject reg = registryEntryForDevice(m_selectedDevice);
    QJsonArray list = reg.value(QStringLiteral("accessList")).toArray();
    for (const QJsonValue &v : list) {
        if (v.toObject().value(QStringLiteral("fingerId")).toInt() == newFingerId) {
            emit notificationReceived(QStringLiteral("Parmak zaten listede"), true);
            return false;
        }
    }
    if (list.size() >= 4) {
        emit notificationReceived(QStringLiteral("Maximum of 4 access entries per vault"), true);
        return false;
    }

    QJsonObject entry;
    entry.insert(QStringLiteral("fingerId"), newFingerId);
    entry.insert(QStringLiteral("isOwner"), false);
    entry.insert(QStringLiteral("label"),
                 QStringLiteral("Kullanıcı %1").arg(newFingerId));
    list.append(entry);
    reg.insert(QStringLiteral("accessList"), list);
    if (!saveRegistryEntry(reg)) {
        emit notificationReceived(QStringLiteral("Could not update vault registry"), true);
        return false;
    }

    addLog(QStringLiteral("SUCCESS"),
           QStringLiteral("Erişim verildi: finger %1").arg(newFingerId));
    emit notificationReceived(QStringLiteral("Erişim listesine eklendi"), false);
    emit statusChanged();
    return true;
}

bool VaultController::revokeAccess(int ownerFingerId, int slotNumber)
{
    Q_UNUSED(ownerFingerId);
    if (m_selectedDevice.isEmpty()) {
        emit notificationReceived(QStringLiteral("Cihaz seçilmedi"), true);
        return false;
    }

    const int fingerId = slotNumber; /* legacy param: fingerId, not LUKS slot */
    QJsonObject reg = registryEntryForDevice(m_selectedDevice);
    QJsonArray list = reg.value(QStringLiteral("accessList")).toArray();
    QJsonArray next;
    bool removed = false;
    for (const QJsonValue &v : list) {
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("fingerId")).toInt() == fingerId) {
            if (o.value(QStringLiteral("isOwner")).toBool()) {
                emit notificationReceived(QStringLiteral("Sahip kaldırılamaz"), true);
                return false;
            }
            removed = true;
            continue;
        }
        next.append(o);
    }
    if (!removed) {
        emit notificationReceived(QStringLiteral("Entry not found"), true);
        return false;
    }
    reg.insert(QStringLiteral("accessList"), next);
    saveRegistryEntry(reg);
    addLog(QStringLiteral("WARNING"),
           QStringLiteral("Erişim kaldırıldı: finger %1").arg(fingerId));
    emit notificationReceived(QStringLiteral("Erişim listesinden çıkarıldı"), false);
    emit statusChanged();
    return true;
}

QVariantList VaultController::accessListForDevice(const QString &devicePath) const
{
    QVariantList out;
    const QJsonObject reg = registryEntryForDevice(devicePath);
    QJsonArray list = reg.value(QStringLiteral("accessList")).toArray();
    if (list.isEmpty()) {
        const int owner = reg.value(QStringLiteral("ownerFingerId")).toInt(-1);
        if (owner > 0) {
            QVariantMap m;
            m.insert(QStringLiteral("fingerId"), owner);
            m.insert(QStringLiteral("number"), owner);
            m.insert(QStringLiteral("label"), QStringLiteral("Owner"));
            m.insert(QStringLiteral("isOwner"), true);
            out << m;
        }
        return out;
    }
    for (const QJsonValue &v : list) {
        const QJsonObject o = v.toObject();
        QVariantMap m;
        const int fid = o.value(QStringLiteral("fingerId")).toInt();
        m.insert(QStringLiteral("fingerId"), fid);
        m.insert(QStringLiteral("number"), fid);
        m.insert(QStringLiteral("label"), o.value(QStringLiteral("label")).toString());
        m.insert(QStringLiteral("isOwner"), o.value(QStringLiteral("isOwner")).toBool());
        out << m;
    }
    return out;
}

QVariantList VaultController::fetchKeyslotsForDevice(const QString &devicePath) const
{
    QVariantList slotList;
    if (devicePath.isEmpty())
        return slotList;

    const QString bin = findVaultManager();
    if (bin.isEmpty())
        return slotList;

    QProcess proc;
    proc.start(QStringLiteral("pkexec"), QStringList() << bin << QStringLiteral("listslots") << devicePath);
    if (!proc.waitForFinished(10000))
        return slotList;

    const QString output = QString::fromUtf8(proc.readAllStandardOutput());
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        if (!line.startsWith(QStringLiteral("SLOT:")))
            continue;
        const QStringList parts = line.mid(5).split(QLatin1Char(':'));
        if (parts.size() < 2)
            continue;
        QVariantMap slot;
        const int slotNum = parts[0].toInt();
        slot.insert(QStringLiteral("number"), slotNum);
        slot.insert(QStringLiteral("type"), parts[1]);
        slot.insert(QStringLiteral("label"),
                    slotNum == 0 ? QStringLiteral("Owner (Primary)")
                                 : QStringLiteral("User Slot %1").arg(slotNum));
        slot.insert(QStringLiteral("isOwner"), slotNum == 0);
        slotList << slot;
    }

    return slotList;
}

QVariantList VaultController::getKeyslots()
{
    return accessListForDevice(m_selectedDevice);
}

QVariantList VaultController::getKeyslotsForDevice(const QString &devicePath)
{
    return accessListForDevice(devicePath);
}

QString VaultController::resolveManagedDiskPath(const QString &registryPath) const
{
    if (registryPath.isEmpty())
        return QString();

    QProcess lsblk;
    lsblk.start(QStringLiteral("lsblk"),
                {QStringLiteral("-no"), QStringLiteral("TYPE,PKNAME"), registryPath});
    if (lsblk.waitForFinished(2000) && lsblk.exitCode() == 0) {
        const QStringList parts =
            QString::fromUtf8(lsblk.readAllStandardOutput()).trimmed().split(
                QLatin1Char(' '), Qt::SkipEmptyParts);
        const QString type = parts.value(0);
        if (type == QStringLiteral("disk") || type == QStringLiteral("loop"))
            return registryPath;
        if (parts.size() >= 2 && !parts.at(1).isEmpty())
            return QStringLiteral("/dev/") + parts.at(1);
    }

    if (registryPath.startsWith(QStringLiteral("/dev/loop"))
        && !registryPath.contains(QLatin1Char('p')))
        return registryPath;

    return registryPath;
}

QString VaultController::findLuksPartitionPath(const QString &devicePath) const
{
    if (devicePath.isEmpty())
        return QString();

    QProcess lsblk;
    lsblk.start(QStringLiteral("lsblk"),
                {QStringLiteral("-Jbno"), QStringLiteral("FSTYPE,NAME,TYPE"), devicePath});
    if (!lsblk.waitForFinished(3000) || lsblk.exitCode() != 0)
        return QString();

    const QJsonDocument doc = QJsonDocument::fromJson(lsblk.readAllStandardOutput());
    if (!doc.isObject())
        return QString();

    const QJsonArray devs = doc.object()[QStringLiteral("blockdevices")].toArray();
    if (devs.isEmpty())
        return QString();

    std::function<QString(const QJsonObject &)> walk;
    walk = [&walk](const QJsonObject &node) -> QString {
        if (node.value(QStringLiteral("fstype")).toString() == QStringLiteral("crypto_LUKS")) {
            return QStringLiteral("/dev/") + node.value(QStringLiteral("name")).toString();
        }
        const QJsonArray children = node.value(QStringLiteral("children")).toArray();
        for (const QJsonValue &v : children) {
            const QString found = walk(v.toObject());
            if (!found.isEmpty())
                return found;
        }
        return QString();
    };

    const QString found = walk(devs.at(0).toObject());
    if (!found.isEmpty())
        return found;

    if (devs.at(0).toObject().value(QStringLiteral("fstype")).toString()
        == QStringLiteral("crypto_LUKS"))
        return devicePath;

    return QString();
}

bool VaultController::isRegistryManagedPath(const QString &path) const
{
    if (path.isEmpty())
        return false;

    for (const QJsonValue &v : loadRegistryArray()) {
        const QString rp = v.toObject().value(QStringLiteral("path")).toString();
        if (rp == path)
            return true;
        const QString disk = resolveManagedDiskPath(rp);
        if (disk == path)
            return true;
        if (findLuksPartitionPath(rp) == path)
            return true;
    }
    return false;
}

bool VaultController::renameVaultDisplayName(const QString &devicePath, const QString &newName)
{
    const QString trimmed = newName.trimmed();
    if (trimmed.isEmpty()) {
        emit notificationReceived(QStringLiteral("Display name cannot be empty."), true);
        return false;
    }
    if (trimmed.size() > 64) {
        emit notificationReceived(QStringLiteral("Display name is too long (max 64 characters)."), true);
        return false;
    }

    QJsonObject reg = registryEntryForDevice(devicePath);
    if (reg.isEmpty()) {
        reg.insert(QStringLiteral("path"), devicePath);
    }
    reg.insert(QStringLiteral("vaultName"), trimmed);
    saveRegistryEntry(reg);
    refreshStorageDevices();
    emit devicesChanged();
    addLog("INFO", QStringLiteral("Renamed vault display name: ") + trimmed);
    return true;
}

namespace {

QString formatBlockSize(qlonglong bytes)
{
    if (bytes <= 0)
        return QStringLiteral("—");
    const double kb = 1024.0;
  const double mb = kb * 1024.0;
    const double gb = mb * 1024.0;
    if (bytes >= gb)
        return QStringLiteral("%1G").arg(bytes / gb, 0, 'f', bytes >= 10 * gb ? 0 : 1);
    if (bytes >= mb)
        return QStringLiteral("%1M").arg(bytes / mb, 0, 'f', bytes >= 10 * mb ? 0 : 1);
    if (bytes >= kb)
        return QStringLiteral("%1K").arg(bytes / kb, 0, 'f', 0);
    return QStringLiteral("%1B").arg(bytes);
}

qlonglong bytesFromLsblkValue(const QJsonValue &sizeVal)
{
    if (sizeVal.isDouble())
        return static_cast<qlonglong>(sizeVal.toDouble());
    if (sizeVal.isString())
        return sizeVal.toString().toLongLong();
    return sizeVal.toVariant().toLongLong();
}

QString sizeDisplayFromLsblk(const QJsonObject &node)
{
    const QJsonValue sizeVal = node.value(QStringLiteral("size"));
    const qlonglong bytes = bytesFromLsblkValue(sizeVal);
    if (!sizeVal.isString())
        return formatBlockSize(bytes);
    const QString s = sizeVal.toString();
    return s.isEmpty() ? formatBlockSize(bytes) : s;
}

QString fsLabelForFstype(const QString &fstype)
{
    if (fstype == QStringLiteral("crypto_LUKS"))
        return QStringLiteral("LUKS2");
    if (fstype == QStringLiteral("vfat"))
        return QStringLiteral("FAT32");
    if (fstype.isEmpty())
        return QStringLiteral("—");
    return fstype;
}

QString mountDisplay(const QString &mountpoint)
{
    if (mountpoint.isEmpty())
        return QStringLiteral("(not mounted)");
    return mountpoint;
}

void applyUsageUnknown(QVariantMap &part)
{
    part.insert(QStringLiteral("usedBytes"), -1);
    part.insert(QStringLiteral("unusedBytes"), -1);
    part.insert(QStringLiteral("usedLabel"), QStringLiteral("—"));
    part.insert(QStringLiteral("unusedLabel"), QStringLiteral("—"));
    part.insert(QStringLiteral("usedRatio"), -1.0);
}

void setPartitionUsage(QVariantMap &part, qlonglong usedBytes, qlonglong partBytes)
{
    if (usedBytes < 0 || partBytes <= 0) {
        applyUsageUnknown(part);
        return;
    }
    usedBytes = qBound<qlonglong>(0, usedBytes, partBytes);
    const qlonglong unusedBytes = partBytes - usedBytes;
    part.insert(QStringLiteral("usedBytes"), usedBytes);
    part.insert(QStringLiteral("unusedBytes"), unusedBytes);
    part.insert(QStringLiteral("usedLabel"), formatBlockSize(usedBytes));
    part.insert(QStringLiteral("unusedLabel"), formatBlockSize(unusedBytes));
    part.insert(QStringLiteral("usedRatio"),
                static_cast<double>(usedBytes) / static_cast<double>(partBytes));
}

void tryLsblkUsage(QVariantMap &part, const QJsonObject &node)
{
    const QString mountpoint = part.value(QStringLiteral("mountpoint")).toString();
    if (mountpoint.isEmpty()) {
        applyUsageUnknown(part);
        return;
    }

    const QJsonValue fsusedVal = node.value(QStringLiteral("fsused"));
    if (fsusedVal.isNull() || fsusedVal.isUndefined()) {
        applyUsageUnknown(part);
        return;
    }

    const qlonglong partBytes = part.value(QStringLiteral("bytes")).toLongLong();
    const qlonglong fsused = bytesFromLsblkValue(fsusedVal);
    if (fsused < 0) {
        applyUsageUnknown(part);
        return;
    }
    setPartitionUsage(part, fsused, partBytes);
}

QVariantMap makePartitionEntry(const QJsonObject &node,
                               const QString &partPath,
                               qlonglong diskBytes)
{
    const QString fstype = node.value(QStringLiteral("fstype")).toString();
    const qlonglong bytes = bytesFromLsblkValue(node.value(QStringLiteral("size")));
    const QString size = sizeDisplayFromLsblk(node);
    const QString mountpoint = node.value(QStringLiteral("mountpoint")).toString();

    QString role = QStringLiteral("storage");
    QString roleLabel = QStringLiteral("Normal storage");
    QString label = node.value(QStringLiteral("label")).toString();

    if (fstype == QStringLiteral("crypto_LUKS")) {
        role = QStringLiteral("luks");
        roleLabel = QStringLiteral("Encrypted vault");
        if (label.isEmpty())
            label = roleLabel;
    } else if (label.isEmpty()) {
        label = fstype.isEmpty() ? QStringLiteral("Partition") : fstype;
    }

    QVariantMap part;
    part.insert(QStringLiteral("path"), partPath);
    part.insert(QStringLiteral("size"), size);
    part.insert(QStringLiteral("bytes"), bytes);
    part.insert(QStringLiteral("fstype"), fstype);
    part.insert(QStringLiteral("fsLabel"), fsLabelForFstype(fstype));
    part.insert(QStringLiteral("mountpoint"), mountpoint);
    part.insert(QStringLiteral("mountDisplay"), mountDisplay(mountpoint));
    part.insert(QStringLiteral("role"), role);
    part.insert(QStringLiteral("roleLabel"), roleLabel);
    part.insert(QStringLiteral("label"), label);
    if (diskBytes > 0) {
        part.insert(QStringLiteral("ratio"), static_cast<double>(bytes) / static_cast<double>(diskBytes));
        part.insert(QStringLiteral("percentOfDisk"),
                    QStringLiteral("%1%").arg(qRound(100.0 * bytes / diskBytes)));
    }
    tryLsblkUsage(part, node);
    return part;
}

QString buildUsedSummary(const QVariantList &parts)
{
    QStringList chunks;
    for (const QVariant &v : parts) {
        const QVariantMap p = v.toMap();
        const QString role = p.value(QStringLiteral("role")).toString();
        const QString size = p.value(QStringLiteral("size")).toString();
        if (role == QStringLiteral("luks"))
            chunks << QStringLiteral("%1 encrypted").arg(size);
        else if (role == QStringLiteral("container"))
            chunks << QStringLiteral("%1 container").arg(size);
        else if (role == QStringLiteral("storage"))
            chunks << QStringLiteral("%1 normal").arg(size);
    }
    return chunks.join(QStringLiteral(", "));
}

} // namespace

QVariantMap VaultController::previewDeleteVault(const QString &devicePath) const
{
    QVariantMap preview;
    preview.insert(QStringLiteral("devicePath"), devicePath);

    QStringList deleted;
    QStringList kept;
    QStringList registryNotes;

    const QJsonObject reg = registryEntryForDevice(devicePath);
    const QString backing = reg.value(QStringLiteral("backing")).toString();
    const bool isLoop = !backing.isEmpty();

    if (isLoop) {
        deleted << QStringLiteral("Loop device %1").arg(devicePath);
        deleted << QStringLiteral("Sparse image %1").arg(backing);
        registryNotes << QStringLiteral("Registry entry for %1").arg(devicePath);
        preview.insert(QStringLiteral("confirmHint"), reg.value(QStringLiteral("vaultName")).toString());
    } else {
        const QString disk = resolveManagedDiskPath(devicePath);
        const QVariantMap layout = storageLayoutForDevice(disk.isEmpty() ? devicePath : disk);
        const QString luksPart = findLuksPartitionPath(devicePath);
        const QVariantList parts = layout.value(QStringLiteral("partitions")).toList();

        for (const QVariant &v : parts) {
            const QVariantMap part = v.toMap();
            const QString p = part.value(QStringLiteral("path")).toString();
            const QString role = part.value(QStringLiteral("role")).toString();
            const QString size = part.value(QStringLiteral("size")).toString();
            const QString roleLabel = part.value(QStringLiteral("roleLabel")).toString();
            const QString fsLabel = part.value(QStringLiteral("fsLabel")).toString();

            if (role == QStringLiteral("luks") || p == luksPart
                || (role == QStringLiteral("container") && reg.value(QStringLiteral("isVault")).toBool())) {
                if (role == QStringLiteral("container") && !reg.value(QStringLiteral("isVault")).toBool())
                    continue;
                deleted << QStringLiteral("%1 %2 (%3, %4) — all vault data permanently destroyed")
                               .arg(p, roleLabel, size, fsLabel);
            } else if (!p.isEmpty() && role != QStringLiteral("container")) {
                kept << QStringLiteral("%1 %2 (%3, %4) — data unchanged")
                            .arg(p, roleLabel, size, fsLabel);
            }
        }

        if (deleted.isEmpty() && !luksPart.isEmpty()) {
            deleted << QStringLiteral("%1 — encrypted vault data permanently destroyed").arg(luksPart);
        }

        if (!reg.isEmpty())
            registryNotes << QStringLiteral("Vault registry entry removed or cleared");
        preview.insert(QStringLiteral("confirmHint"),
                       reg.value(QStringLiteral("vaultName")).toString().isEmpty()
                           ? QStringLiteral("DELETE")
                           : reg.value(QStringLiteral("vaultName")).toString());
    }

    if (!preview.contains(QStringLiteral("confirmHint"))
        || preview.value(QStringLiteral("confirmHint")).toString().isEmpty())
        preview.insert(QStringLiteral("confirmHint"), QStringLiteral("DELETE"));

    preview.insert(QStringLiteral("deleted"), QVariant::fromValue(deleted));
    preview.insert(QStringLiteral("kept"), QVariant::fromValue(kept));
    preview.insert(QStringLiteral("registry"), QVariant::fromValue(registryNotes));
    preview.insert(QStringLiteral("after"),
                   QStringLiteral("Device disappears from Managed vaults (or returns to Connected storage)."));
    return preview;
}

QVariantMap VaultController::storageLayoutForDevice(const QString &diskPath) const
{
    QVariantMap layout;
    if (diskPath.isEmpty())
        return layout;

    const QString disk = resolveManagedDiskPath(diskPath);
    const QString queryPath = disk.isEmpty() ? diskPath : disk;
    layout.insert(QStringLiteral("diskPath"), queryPath);

    const QJsonObject reg = registryEntryForDevice(diskPath);

    QProcess lsblk;
    lsblk.start(QStringLiteral("lsblk"),
                {QStringLiteral("-Jbno"),
                 QStringLiteral("NAME,SIZE,TYPE,FSTYPE,MOUNTPOINT,LABEL,MODEL,FSUSED,FSAVAIL,FSUSE%"),
                 queryPath});
    if (!lsblk.waitForFinished(3000) || lsblk.exitCode() != 0)
        return layout;

    const QJsonDocument doc = QJsonDocument::fromJson(lsblk.readAllStandardOutput());
    if (!doc.isObject())
        return layout;

    const QJsonArray devs = doc.object()[QStringLiteral("blockdevices")].toArray();
    if (devs.isEmpty())
        return layout;

    const QJsonObject root = devs.at(0).toObject();
    const QString rootType = root.value(QStringLiteral("type")).toString();
    const QString rootFstype = root.value(QStringLiteral("fstype")).toString();
    const qlonglong diskBytes = bytesFromLsblkValue(root.value(QStringLiteral("size")));
    const QString diskSize = sizeDisplayFromLsblk(root);

    layout.insert(QStringLiteral("diskSize"), diskSize);
    layout.insert(QStringLiteral("diskSizeBytes"), diskBytes);
    layout.insert(QStringLiteral("diskType"), rootType);

    QString diskModel = root.value(QStringLiteral("model")).toString().trimmed();
    if (diskModel.isEmpty() && rootType == QStringLiteral("loop")) {
        const QString backing = reg.value(QStringLiteral("backing")).toString();
        diskModel = backing.isEmpty()
                        ? QStringLiteral("Virtual container")
                        : QStringLiteral("Virtual container — ") + QFileInfo(backing).fileName();
    }
    if (diskModel.isEmpty())
        diskModel = QStringLiteral("Storage device");
    layout.insert(QStringLiteral("diskModel"), diskModel);

    QVariantList parts;
    const QJsonArray children = root.value(QStringLiteral("children")).toArray();
    for (const QJsonValue &v : children) {
        const QJsonObject child = v.toObject();
        if (child.value(QStringLiteral("type")).toString() != QStringLiteral("part"))
            continue;
        const QString partPath = QStringLiteral("/dev/") + child.value(QStringLiteral("name")).toString();
        parts << makePartitionEntry(child, partPath, diskBytes);
    }

    if (parts.isEmpty() && rootFstype == QStringLiteral("crypto_LUKS")) {
        parts << makePartitionEntry(root, queryPath, diskBytes);
    }

    if (parts.isEmpty() && rootType == QStringLiteral("loop")) {
        QVariantMap container;
        const int registrySizeMB = reg.value(QStringLiteral("sizeMB")).toInt();
        qlonglong bytes = diskBytes;
        if (bytes <= 0 && registrySizeMB > 0)
            bytes = static_cast<qlonglong>(registrySizeMB) * 1024 * 1024;
        const QString size = bytes > 0 ? formatBlockSize(bytes) : diskSize;
        const bool encrypted = reg.value(QStringLiteral("isVault")).toBool()
                               || findLuksPartitionPath(queryPath) == queryPath;

        container.insert(QStringLiteral("path"), queryPath);
        container.insert(QStringLiteral("size"), size);
        container.insert(QStringLiteral("bytes"), bytes);
        container.insert(QStringLiteral("fstype"), rootFstype);
        container.insert(QStringLiteral("fsLabel"), encrypted ? QStringLiteral("LUKS2") : QStringLiteral("—"));
        container.insert(QStringLiteral("mountpoint"), QString());
        container.insert(QStringLiteral("mountDisplay"), mountDisplay(QString()));
        container.insert(QStringLiteral("role"), QStringLiteral("container"));
        container.insert(QStringLiteral("roleLabel"),
                         encrypted ? QStringLiteral("Encrypted vault")
                                   : QStringLiteral("Container (not encrypted)"));
        container.insert(QStringLiteral("label"), container.value(QStringLiteral("roleLabel")));
        if (diskBytes > 0 && bytes > 0) {
            container.insert(QStringLiteral("ratio"), static_cast<double>(bytes) / static_cast<double>(diskBytes));
            container.insert(QStringLiteral("percentOfDisk"), QStringLiteral("100%"));
        }
        applyUsageUnknown(container);
        parts << container;
    }

    if (isVaultUnlockedForDevice(diskPath)) {
        const QStorageInfo storage(m_mountPoint);
        if (storage.isValid() && storage.isReady() && storage.bytesTotal() > 0) {
            const qlonglong fsUsed = storage.bytesTotal() - storage.bytesAvailable();
            for (int i = 0; i < parts.size(); ++i) {
                QVariantMap part = parts.at(i).toMap();
                if (part.value(QStringLiteral("role")).toString() != QStringLiteral("luks"))
                    continue;
                const qlonglong partBytes = part.value(QStringLiteral("bytes")).toLongLong();
                setPartitionUsage(part, fsUsed, partBytes > 0 ? partBytes : storage.bytesTotal());
                parts[i] = part;
            }
        }
    }

    layout.insert(QStringLiteral("partitions"), parts);
    layout.insert(QStringLiteral("partitionCount"), parts.size());
    layout.insert(QStringLiteral("usedSummary"), buildUsedSummary(parts));
    layout.insert(QStringLiteral("hasOpenVault"), isVaultUnlockedForDevice(diskPath));
    return layout;
}

QVariantList VaultController::availablePartitionActions(const QString &diskPath,
                                                        const QString &partPath) const
{
    Q_UNUSED(partPath);
    QVariantList actions;
    const QVariantMap layout = storageLayoutForDevice(diskPath);
    const QVariantList parts = layout.value(QStringLiteral("partitions")).toList();

    auto add = [&](const QString &id, const QString &label, bool dangerous) {
        QVariantMap a;
        a.insert(QStringLiteral("id"), id);
        a.insert(QStringLiteral("label"), label);
        a.insert(QStringLiteral("dangerous"), dangerous);
        actions << a;
    };

    add(QStringLiteral("setup_encryption"), QStringLiteral("Set up encryption"), false);
    if (!parts.isEmpty())
        add(QStringLiteral("repartition_disk"), QStringLiteral("Repartition entire disk"), true);

    bool hasLuks = false;
    bool hasNormal = false;
    for (const QVariant &v : parts) {
        const QString role = v.toMap().value(QStringLiteral("role")).toString();
        if (role == QStringLiteral("luks"))
            hasLuks = true;
        else
            hasNormal = true;
    }

    if (hasLuks)
        add(QStringLiteral("delete_vault"), QStringLiteral("Delete vault"), true);
    if (hasNormal)
        add(QStringLiteral("format_normal"), QStringLiteral("Format normal partition"), true);
    if (parts.size() >= 2)
        add(QStringLiteral("merge_partitions"), QStringLiteral("Merge partitions…"), true);

    return actions;
}

QVariantMap VaultController::buildStoragePlan(const QString &actionId,
                                              const QVariantMap &params) const
{
    QVariantMap plan;
    plan.insert(QStringLiteral("actionId"), actionId);
    plan.insert(QStringLiteral("diskPath"), params.value(QStringLiteral("diskPath")));
    plan.insert(QStringLiteral("partitionPath"), params.value(QStringLiteral("partitionPath")));
    plan.insert(QStringLiteral("params"), params);
    return plan;
}

QVariantMap VaultController::previewStoragePlan(const QVariantMap &plan) const
{
    const QString actionId = plan.value(QStringLiteral("actionId")).toString();
    const QString diskPath = plan.value(QStringLiteral("diskPath")).toString();

    if (actionId == QStringLiteral("delete_vault"))
        return previewDeleteVault(diskPath);

    QVariantMap preview;
    preview.insert(QStringLiteral("deleted"),
                   QVariant::fromValue(QStringList{QStringLiteral("Planned: %1").arg(actionId)}));
    preview.insert(QStringLiteral("kept"), QVariant::fromValue(QStringList{}));
    preview.insert(QStringLiteral("registry"), QVariant::fromValue(QStringList{}));
    preview.insert(QStringLiteral("after"), QStringLiteral("Changes applied after confirmation."));
    return preview;
}

bool VaultController::applyStoragePlan(const QVariantMap &plan)
{
    const QString actionId = plan.value(QStringLiteral("actionId")).toString();
    const QString diskPath = plan.value(QStringLiteral("diskPath")).toString();

    if (actionId == QStringLiteral("delete_vault"))
        return deleteVault(diskPath);

    emit storagePlanFinished(false,
                             QStringLiteral("Action '%1' is not available yet.").arg(actionId));
    emit notificationReceived(QStringLiteral("This storage action is planned for a future release."),
                              true);
    return false;
}

QStringList VaultController::supportedFilesystems() const
{
    return {QStringLiteral("exfat"), QStringLiteral("ext4")};
}

QVariantList VaultController::managedDevices() const
{
    QVariantList out;
    QSet<QString> seen;

    auto appendEntry = [&](const QVariantMap &base) {
        const QString path = base.value(QStringLiteral("path")).toString();
        if (path.isEmpty() || seen.contains(path))
            return;
        seen.insert(path);

        QVariantMap entry = base;
        const QJsonObject reg = registryEntryForDevice(path);
        QString displayName = reg.value(QStringLiteral("vaultName")).toString();
        if (displayName.isEmpty())
            displayName = reg.value(QStringLiteral("label")).toString();
        if (displayName.isEmpty())
            displayName = entry.value(QStringLiteral("name")).toString();
        entry.insert(QStringLiteral("displayName"), displayName);

        QString statusLabel = QStringLiteral("Unavailable");
        if (!entry.value(QStringLiteral("valid")).toBool()) {
            statusLabel = QStringLiteral("Unavailable");
        } else if (isVaultUnlockedForDevice(path)) {
            statusLabel = QStringLiteral("Unlocked");
        } else if (entry.value(QStringLiteral("isVault")).toBool()) {
            statusLabel = QStringLiteral("Locked");
        } else if (!reg.isEmpty()) {
            statusLabel = QStringLiteral("Container");
        } else {
            statusLabel = QStringLiteral("Encrypted");
        }
        if (!entry.contains(QStringLiteral("name")) || entry.value(QStringLiteral("name")).toString().isEmpty())
            entry.insert(QStringLiteral("name"), displayName);
        entry.insert(QStringLiteral("statusLabel"), statusLabel);
        entry.insert(QStringLiteral("canDelete"), true);
        entry.insert(QStringLiteral("canRename"), !reg.isEmpty() || entry.value(QStringLiteral("isVault")).toBool());
        out << entry;
    };

    for (const QJsonValue &v : loadRegistryArray()) {
        const QString path = v.toObject().value(QStringLiteral("path")).toString();
        QVariantMap info = resolveDeviceInfo(path);
        if (!info.value(QStringLiteral("valid")).toBool())
            info = resolveDeviceInfo(resolveManagedDiskPath(path));
        appendEntry(info);
    }

    for (const QVariant &v : m_storageDevices) {
        const QVariantMap e = v.toMap();
        if (!e.value(QStringLiteral("isVault")).toBool())
            continue;
        const QString path = e.value(QStringLiteral("path")).toString();
        if (isRegistryManagedPath(path))
            continue;
        appendEntry(enrichVaultEntry(e));
    }

    return out;
}

QVariantList VaultController::discoveredStorageDevices() const
{
    QVariantList out;
    for (const QVariant &v : m_storageDevices) {
        const QVariantMap e = v.toMap();
        const QString kind = e.value(QStringLiteral("kind")).toString();
        if (kind != QStringLiteral("disk") && kind != QStringLiteral("partition"))
            continue;
        if (e.value(QStringLiteral("isVault")).toBool())
            continue;
        const QString path = e.value(QStringLiteral("path")).toString();
        if (isRegistryManagedPath(path))
            continue;
        out << e;
    }
    return out;
}

QVariantMap VaultController::enrichVaultEntry(const QVariantMap &entry) const
{
    QVariantMap out = entry;
    const QString path = entry.value(QStringLiteral("path")).toString();
    const QJsonObject reg = registryEntryForDevice(path);

    QString displayName = reg.value(QStringLiteral("vaultName")).toString();
    if (displayName.isEmpty())
        displayName = reg.value(QStringLiteral("label")).toString();
    if (displayName.isEmpty())
        displayName = entry.value(QStringLiteral("name")).toString();
    out.insert(QStringLiteral("displayName"), displayName);

    QString sizeLabel = entry.value(QStringLiteral("luksSize")).toString();
    if (sizeLabel.isEmpty()) {
        const int capacityMB = entry.value(QStringLiteral("capacityMB")).toInt();
        if (capacityMB > 0)
            sizeLabel = QStringLiteral("%1 GB").arg(capacityMB / 1024.0, 0, 'f', 1);
    }
    out.insert(QStringLiteral("sizeLabel"), sizeLabel);
    out.insert(QStringLiteral("encryptionLabel"), QStringLiteral("AES-256-XTS (LUKS2)"));
    out.insert(QStringLiteral("devicePath"), path);

    const bool selected = (path == m_selectedDevice);
    out.insert(QStringLiteral("isSelected"), selected);
    out.insert(QStringLiteral("isUnlocked"), isVaultUnlockedForDevice(path));
    return out;
}

QVariantList VaultController::registeredVaults() const
{
    QVariantList vaults;
    for (const QVariant &v : m_storageDevices) {
        const QVariantMap entry = v.toMap();
        if (!entry.value(QStringLiteral("isVault")).toBool())
            continue;
        vaults << enrichVaultEntry(entry);
    }
    return vaults;
}

QVariantMap VaultController::vaultDetailForDevice(const QString &devicePath) const
{
    QVariantMap detail;
    if (devicePath.isEmpty())
        return detail;

    for (const QVariant &v : m_storageDevices) {
        const QVariantMap entry = v.toMap();
        if (entry.value(QStringLiteral("path")).toString() != devicePath)
            continue;
        detail = enrichVaultEntry(entry);
        break;
    }

    if (detail.isEmpty()) {
        detail = enrichVaultEntry(resolveDeviceInfo(devicePath));
    }

    const QVariantList accessList = accessListForDevice(devicePath);
    detail.insert(QStringLiteral("accessCount"), accessList.size());
    detail.insert(QStringLiteral("keyslots"), accessList);
    detail.insert(QStringLiteral("accessList"), accessList);
    detail.insert(QStringLiteral("path"), devicePath);
    detail.insert(QStringLiteral("isUnlocked"), isVaultUnlockedForDevice(devicePath));
    return detail;
}

bool VaultController::isVaultUnlockedForDevice(const QString &devicePath) const
{
    if (devicePath.isEmpty() || m_unlockedDevice.isEmpty())
        return false;
    return devicePath == m_unlockedDevice && isVaultRuntimeOpen();
}

void VaultController::setAutoLockTimeout(int minutes)
{
    if (m_autoLockTimeout == minutes) return;
    m_autoLockTimeout = minutes;
    addLog("INFO", QString("Auto-lock timeout changed to %1 minutes").arg(minutes == 0 ? "Never" : QString::number(minutes)));
    resetAutoLockTimer();
    emit autoLockTimeoutChanged();
}

void VaultController::resetAutoLockTimer()
{
    if (!m_autoLockTimer) return;
    
    if (m_autoLockTimeout <= 0 || m_isLocked) {
        m_autoLockTimer->stop();
        return;
    }
    
    m_autoLockTimer->start(m_autoLockTimeout * 60 * 1000);
}

void VaultController::handleAutoLock()
{
    if (m_isToggling || !isVaultRuntimeOpen())
        return;
    addLog("WARNING", "Inactivity timeout reached. Auto-locking vault...");
    beginLockVault();
}
