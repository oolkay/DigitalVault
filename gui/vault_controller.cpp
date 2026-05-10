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
#include <QtConcurrent/QtConcurrent>

/* Unix socket + HAL iletişimi için */
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

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
    , m_mountPoint(QDir::homePath() + "/VaultMount")
    , m_lastActivity("—")
    , m_socketPath("/tmp/biometric_vault.sock")
    , m_simProcess(nullptr)
    , m_pollTimer(nullptr)
    , m_setupWatcher(nullptr)
    , m_selectedDevice("vault.img")
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
    if (!m_isLocked) {
        unmountVault();
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
    updateActivity("Simulator process ended.");
    emit statusChanged();
}

/* ─── Vault Mount/Unmount (LUKS + ext4) ──────────────────────────────────── */

bool VaultController::mountVault()
{
    addLog("INFO", "Starting LUKS unlock for: " + m_selectedDevice);

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
        addLog("SUCCESS", "Vault locked. LUKS keys cleared from kernel.");
        return true;
    }

    addLog("ERROR", "Failed to close vault (exit code: " + QString::number(ret) + ")");
    return false;
}

void VaultController::toggleVault()
{
    if (m_isToggling) return;
    resetAutoLockTimer(); // Kullanıcı etkileşimi timer'ı sıfırlar

    if (m_isLocked) {
        /* Unlock — mount vault */
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
            bool success = watcher->result();
            m_isToggling = false;
            emit togglingChanged();

            if (success) {
                m_isLocked = false;
                QString msg = "Vault unlocked and mounted at " + m_mountPoint;
                updateActivity(msg);
                emit notificationReceived(msg, false);
                resetAutoLockTimer(); // Başarıyla açıldıktan sonra sayacı başlat
            } else {
                emit notificationReceived("Failed to unlock vault!", true);
                updateActivity("Unlock failed.");
            }
            emit statusChanged();
            watcher->deleteLater();
        });
        watcher->setFuture(future);

    } else {
        /* Lock — unmount vault */
        m_isToggling = true;
        emit togglingChanged();
        updateActivity("Locking vault...");
        emit statusChanged();

        QFuture<bool> future = QtConcurrent::run(&VaultController::unmountVault, this);
        auto watcher = new QFutureWatcher<bool>(this);
        connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher]() {
            bool success = watcher->result();
            m_isToggling = false;
            emit togglingChanged();

            if (success) {
                m_isLocked = true;
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
}

/* ─── Vault Setup (First-Time LUKS Format) ───────────────────────────────── */

bool VaultController::setupVault(const QString &device, bool force, int ownerId)
{
    if (device.isEmpty()) return false;

    if (m_isSetupRunning) {
        emit notificationReceived("Setup is already in progress!", true);
        return false;
    }

    if (!simulatorRunning()) {
        emit notificationReceived("Start the simulator first!", true);
        return false;
    }

    /* Eğer bir .img dosyası ise ve henüz yoksa, oluştur */
    if (device.endsWith(".img") && !QFile::exists(device)) {
        addLog("INFO", "Creating container file: " + device);
        QProcess dd;
        dd.start("truncate", {"-s", "100M", device});
        if (!dd.waitForFinished(5000)) {
            emit notificationReceived("Failed to create vault file!", true);
            return false;
        }
    }

    /* ── Async başlat ── */
    m_isSetupRunning = true;
    m_setupStatusText = "Formatting LUKS2 container on " + device + "...";
    emit setupRunningChanged();

    addLog("INFO", QString("Formatting new LUKS vault on: %1 %2").arg(device).arg(force ? "(FORCED)" : ""));

    /* QFutureWatcher ile arka plan threadi izle */
    m_setupWatcher = new QFutureWatcher<int>(this);
    connect(m_setupWatcher, &QFutureWatcher<int>::finished, this, [this, device]() {
        int ret = m_setupWatcher->result();
        m_isSetupRunning = false;
        m_setupStatusText = "";
        emit setupRunningChanged();

        m_setupWatcher->deleteLater();
        m_setupWatcher = nullptr;

        if (ret == 0) {
            m_selectedDevice = device;
            addLog("SUCCESS", "Vault setup complete: " + device);
            emit statusChanged();
            emit setupFinished(true, "✅  Vault setup complete! You can now unlock it.");
            emit notificationReceived("Vault initialized! You can now unlock it.", false);
        } else if (ret == 3) {
            addLog("INFO", "Device already has a LUKS vault: " + device);
            emit setupFinished(false, "This device already has a vault. Use 'Unlock' to open it.");
            emit notificationReceived("This device already has a vault! Use 'Open Vault' to unlock it.", false);
        } else if (ret == 4) {
            addLog("ERROR", "Existing filesystem detected on: " + device);
            emit setupFinished(false, "⚠  Existing data detected. Enable 'Force format' to override.");
            emit notificationReceived("Existing data detected! Enable Force Format to override.", true);
        } else {
            addLog("ERROR", "Vault setup failed on: " + device + " (exit " + QString::number(ret) + ")");
            emit setupFinished(false, "❌  Setup failed. Check logs for details.");
            emit notificationReceived("Vault setup failed!", true);
        }
    });

    /* Threadi başlat */
    QStringList args;
    args << "setup" << device << QString::number(ownerId);
    if (force) args << "--force";

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

QVariantList VaultController::getStorageDevices()
{
    QVariantList devices;

    /* 1. Eğer seçili cihaz bir dosya ise (fiziksel disk değilse), onu listeye ekle */
    if (!m_selectedDevice.isEmpty() && !m_selectedDevice.startsWith("/dev/")) {
        QVariantMap d;
        d["path"] = m_selectedDevice;
        QFileInfo fi(m_selectedDevice);
        d["name"] = fi.fileName() + " (File Container)";
        d["description"] = "Virtual Encrypted Container";
        devices << d;
    }

    /* 2. lsblk ile fiziksel diskleri listele */
    QProcess lsblk;
    lsblk.start("lsblk", QStringList() << "-Jno" << "NAME,SIZE,TYPE,MOUNTPOINT,MODEL,VENDOR");
    if (lsblk.waitForFinished(3000)) {
        QJsonDocument doc = QJsonDocument::fromJson(lsblk.readAllStandardOutput());
        if (doc.isObject()) {
            QJsonArray blockDevices = doc.object()["blockdevices"].toArray();
            for (const auto &dev : blockDevices) {
                QJsonObject obj = dev.toObject();
                QString type = obj["type"].toString();

                /* Disk bölümlerini (part), diskleri veya loop cihazlarını ekle */
                if (type == "part" || type == "disk" || type == "loop") {
                    QString mountpoint = obj["mountpoint"].toString();
                    
                    /* Sistem loop cihazlarını (Snap paketleri vb.) gizle */
                    if (type == "loop" && mountpoint.startsWith("/snap")) {
                        continue;
                    }

                    QVariantMap d;
                    QString name = obj["name"].toString();
                    d["path"] = "/dev/" + name;
                    d["name"] = QString("/dev/%1 (%2)").arg(name).arg(obj["size"].toString());

                    QString info = obj["model"].toString() + " " + obj["vendor"].toString();
                    if (info.trimmed().isEmpty()) info = "Physical Storage Device";
                    d["description"] = info;

                    devices << d;
                }
            }
        }
    }

    return devices;
}

/* ─── Periodic Status Poll ───────────────────────────────────────────────── */

void VaultController::pollStatus()
{
    /*
     * LUKS durumunu kontrol et:
     * /dev/mapper/digitalvault varsa vault açık demektir.
     */
    bool wasLocked = m_isLocked;

    if (!m_isLocked) {
        /* /dev/mapper/digitalvault hala var mı? */
        if (!QFile::exists("/dev/mapper/digitalvault")) {
            m_isLocked = true;
            addLog("WARNING", "Vault was closed externally");
        }
    }

    if (wasLocked != m_isLocked) {
        emit statusChanged();
    }
}

bool VaultController::createNewVault(const QString &path)
{
    if (path.isEmpty()) return false;

    /* URL formatındaysa (file://) temizle */
    QString localPath = QUrl(path).toLocalFile();
    if (localPath.isEmpty()) localPath = path;

    m_selectedDevice = localPath;
    updateActivity("New vault target set: " + localPath);
    emit statusChanged();
    emit notificationReceived("Device selected: " + localPath + ". Use 'Setup Vault' to format.", false);

    return true;
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
    if (m_isLocked) {
        emit notificationReceived("Unlock the vault first!", true);
        return;
    }

    QProcess::startDetached("xdg-open", {m_mountPoint});
    resetAutoLockTimer();
    addLog("INFO", "Opened vault in file manager");
}

void VaultController::openInTerminal()
{
    if (m_isLocked) {
        emit notificationReceived("Unlock the vault first!", true);
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

QVariantList VaultController::getKeyslots()
{
    QVariantList slotList;

    if (m_selectedDevice.isEmpty()) return slotList;

    /* vault_manager listslots çıktısını oku */
    QString bin = findVaultManager();
    if (bin.isEmpty()) return slotList;

    QProcess proc;
    proc.start("pkexec", QStringList() << bin << "listslots" << m_selectedDevice);
    if (!proc.waitForFinished(10000)) return slotList;

    QString output = proc.readAllStandardOutput();
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        /* Format: SLOT:0:luks2 */
        if (line.startsWith("SLOT:")) {
            QStringList parts = line.mid(5).split(':');
            if (parts.size() >= 2) {
                QVariantMap slot;
                slot["number"] = parts[0].toInt();
                slot["type"] = parts[1];
                slot["label"] = (parts[0] == "0") ? "Owner (Primary)" : "User Slot " + parts[0];
                slot["isOwner"] = (parts[0] == "0");
                slotList << slot;
            }
        }
    }

    return slotList;
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
    if (!m_isLocked && !m_isToggling) {
        addLog("WARNING", "Inactivity timeout reached. Auto-locking vault...");
        toggleVault();
    }
}
