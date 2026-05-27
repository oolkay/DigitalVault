#include "vault_controller.h"
#include <QDebug>
#include <QTimer>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QFile>
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

/* Unix socket + HAL iletişimi için */
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/wait.h>

VaultController *g_vaultController = nullptr;
std::atomic<bool> VaultController::s_emergencyClosing{false};

static const char kMapperPath[] = "/dev/mapper/digitalvault";

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

bool VaultController::simulatorRunning() const
{
    /* 1. GUI üzerinden başlatılmış mı? */
    if (m_simProcess && m_simProcess->state() == QProcess::Running) {
        return true;
    }

    /* 2. Manuel olarak terminalden başlatılmış mı? (Socket dosyası kontrolü) */
    return QFile::exists(m_socketPath);
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
    if (m_simProcess && m_simProcess->state() == QProcess::Running) {
        emit notificationReceived("Simulator is already running.", false);
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

    m_simProcess->start("python3", QStringList() << simPath << "-v");

    if (!m_simProcess->waitForStarted(3000)) {
        emit notificationReceived("Failed to start simulator!", true);
        return;
    }

    updateActivity("Hardware simulator started.");
    emit statusChanged();
    emit notificationReceived("Simulator started successfully.", false);

    /* Biraz bekleyip parmak izlerini yükle */
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

void VaultController::beginUnlockVault()
{
    if (!simulatorRunning()) {
        emit notificationReceived("Start the simulator first!", true);
        return;
    }

    m_isToggling = true;
    emit togglingChanged();
    updateActivity("Decrypting vault...");
    emit statusChanged();

    QFuture<bool> future = QtConcurrent::run(&VaultController::mountVault, this);
    auto watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher]() {
        const bool success = watcher->result();
        m_isToggling = false;
        emit togglingChanged();

        if (success) {
            m_isLocked = false;
            m_unlockedDevice = m_selectedDevice;
            saveState(QStringLiteral("unlocked"), false);
            const QString msg = QStringLiteral("Vault unlocked and mounted at ") + m_mountPoint;
            updateActivity(msg);
            emit notificationReceived(msg, false);
            resetAutoLockTimer();
        } else {
            emit notificationReceived("Failed to unlock vault!", true);
            updateActivity("Unlock failed.");
        }
        emit statusChanged();
        watcher->deleteLater();
    });
    watcher->setFuture(future);
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
        emit notificationReceived("Setup requires a block device (/dev/...). Create a loop device first.", true);
        return false;
    }

    if (!simulatorRunning()) {
        emit notificationReceived("Start the simulator first!", true);
        return false;
    }

    if (!reconcileVaultRuntime()) {
        emit notificationReceived(
            QStringLiteral("System is busy — could not prepare storage. Close apps using the vault and retry."),
            true);
        return false;
    }

    /* ── Async başlat ── */
    m_isSetupRunning = true;
    m_setupStatusText = "Formatting encrypted vault on " + device + "...";
    saveState(QStringLiteral("locked"), true);
    emit setupRunningChanged();

    addLog("INFO", QString("Formatting new encrypted vault on: %1 %2").arg(device).arg(force ? "(FORCED)" : ""));

    /* QFutureWatcher ile arka plan threadi izle */
    m_setupWatcher = new QFutureWatcher<int>(this);
    connect(m_setupWatcher, &QFutureWatcher<int>::finished, this,
            [this, device, vaultName, ownerId]() {
        int ret = m_setupWatcher->result();
        m_isSetupRunning = false;
        m_setupStatusText = "";
        emit setupRunningChanged();

        m_setupWatcher->deleteLater();
        m_setupWatcher = nullptr;
        saveState(QStringLiteral("locked"), false);
        reconcileVaultRuntime();

        if (ret == 0) {
            m_selectedDevice = device;
            QJsonObject regEntry = registryEntryForDevice(device);
            if (regEntry.isEmpty()) {
                regEntry.insert(QStringLiteral("path"), device);
            }
            regEntry.insert(QStringLiteral("isVault"), true);
            if (!vaultName.isEmpty())
                regEntry.insert(QStringLiteral("vaultName"), vaultName);
            regEntry.insert(QStringLiteral("ownerFingerId"), ownerId);
            saveRegistryEntry(regEntry);
            addLog("SUCCESS", "Vault setup complete: " + device);
            refreshStorageDevices();
            selectDeviceByPath(device, true);
            emit setupFinished(true, "✅ Vault setup complete! You can now unlock it.");
            emit notificationReceived("✅ Vault initialized! You can now unlock it.", false);
            emit navigationRequested(QStringLiteral("VaultManager.qml"));
        } else if (ret == 3) {
            addLog("INFO", "Device already has an encrypted vault: " + device);
            emit setupFinished(false, "This device already has a vault. Use 'Unlock' to open it.");
            emit notificationReceived("This device already has a vault! Use 'Open Vault' to unlock it.", false);
        } else if (ret == 4) {
            addLog("ERROR", "Existing filesystem detected on: " + device);
            emit setupFinished(false, "Existing data detected. Enable 'Force format' to override.");
            emit notificationReceived("Existing data detected! Enable Force Format to override.", true);
        } else {
            addLog("ERROR", "Vault setup failed on: " + device + " (exit " + QString::number(ret) + ")");
            emit setupFinished(false, "Setup failed. Check logs for details.");
            emit notificationReceived("Vault setup failed!", true);
        }
    });

    /* Threadi başlat */
    QStringList args;
    args << "setup" << device << QString::number(ownerId);
    if (force) args << "--force";
    if (vaultSizeMB > 0) args << "--partition" << QString::number(vaultSizeMB);
    if (!vaultName.isEmpty()) args << "--vault-name" << vaultName;
    if (!normalName.isEmpty()) args << "--normal-name" << normalName;
    if (!normalFs.isEmpty()) args << "--normal-fs" << normalFs;

    /* runVaultManager main thread'den çağrılamaz — argümanları kopyalayıp yeni process aç */
    QString vaultManagerBin = m_vaultManagerBin;
    QFuture<int> future = QtConcurrent::run([vaultManagerBin, args]() -> int {
        QProcess proc;
        proc.setProcessChannelMode(QProcess::MergedChannels);
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("SUDO_UID", QString::number(::getuid()));
        env.insert("SUDO_GID", QString::number(::getgid()));
        proc.setProcessEnvironment(env);

        QStringList launchArgs;
        launchArgs << vaultManagerBin << args;

        proc.start("pkexec", launchArgs);
        if (!proc.waitForStarted(5000)) return -1;
        if (!proc.waitForFinished(120000)) { proc.kill(); return -1; } /* 2 dakika timeout */

        QString output = QString::fromUtf8(proc.readAll()).trimmed();
        if (!output.isEmpty()) {
            for (const QString &line : output.split('\n')) {
                printf("[vault_manager] %s\n", line.toStdString().c_str());
            }
        }
        return proc.exitCode();
    });

    m_setupWatcher->setFuture(future);
    return true; /* async başladı, sonuç setupFinished sinyaliyle gelecek */
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
    session.insert(QStringLiteral("defaultOwnerId"), 1);

    return session;
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
        emit notificationReceived("Start the simulator first!", true);
        return false;
    }

    QJsonObject cmd;
    cmd["cmd"] = "verify";
    cmd["finger_id"] = fingerId;

    QByteArray resp = sendSocketCommand(m_socketPath, cmd);
    if (resp.isEmpty()) {
        emit notificationReceived("Failed to communicate with simulator!", true);
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(resp);
    if (doc.object()["status"].toString() == "ok") {
        addLog("SUCCESS", "Owner fingerprint verified (ID: " + QString::number(fingerId) + ")");
        return true;
    } else {
        emit notificationReceived("Verification failed! Fingerprint didn't match.", true);
        return false;
    }
}

bool VaultController::grantAccess(int ownerFingerId, int newFingerId)
{
    if (m_selectedDevice.isEmpty()) {
        emit notificationReceived("No device selected!", true);
        return false;
    }

    if (!simulatorRunning()) {
        emit notificationReceived("Start the simulator first!", true);
        return false;
    }

    addLog("INFO", "Granting new access on " + m_selectedDevice +
           " (owner: " + QString::number(ownerFingerId) +
           ", new: " + QString::number(newFingerId) + ")");

    int ret = runVaultManager({
        "addkey", m_selectedDevice,
        QString::number(ownerFingerId),
        QString::number(newFingerId)
    }, 60000);

    if (ret == 0) {
        addLog("SUCCESS", "New keyslot added for finger ID " + QString::number(newFingerId));
        emit statusChanged();
        emit notificationReceived("Access granted successfully!", false);
        return true;
    }

    addLog("ERROR", "Failed to add keyslot (exit " + QString::number(ret) + ")");
    emit notificationReceived("Failed to grant access. Check logs.", true);
    return false;
}

bool VaultController::revokeAccess(int ownerFingerId, int slotNumber)
{
    if (m_selectedDevice.isEmpty()) {
        emit notificationReceived("No device selected!", true);
        return false;
    }

    if (slotNumber == 0) {
        emit notificationReceived("Cannot remove vault owner (slot 0)!", true);
        return false;
    }

    if (!simulatorRunning()) {
        emit notificationReceived("Start the simulator first!", true);
        return false;
    }

    addLog("WARNING", "Revoking access slot " + QString::number(slotNumber));

    int ret = runVaultManager({
        "killslot", m_selectedDevice,
        QString::number(ownerFingerId),
        QString::number(slotNumber)
    }, 30000);

    if (ret == 0) {
        addLog("SUCCESS", "Keyslot " + QString::number(slotNumber) + " removed");
        emit statusChanged();
        emit notificationReceived("Access slot " + QString::number(slotNumber) + " revoked!", false);
        return true;
    }

    if (ret == 5) {
        emit notificationReceived("Cannot remove vault owner (slot 0)!", true);
        return false;
    }

    addLog("ERROR", "Failed to revoke slot " + QString::number(slotNumber));
    emit notificationReceived("Failed to revoke access.", true);
    return false;
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
    return getKeyslotsForDevice(m_selectedDevice);
}

QVariantList VaultController::getKeyslotsForDevice(const QString &devicePath)
{
    return fetchKeyslotsForDevice(devicePath);
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

    const QVariantList keyslotList = fetchKeyslotsForDevice(devicePath);
    detail.insert(QStringLiteral("accessCount"), keyslotList.size());
    detail.insert(QStringLiteral("keyslots"), keyslotList);
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
