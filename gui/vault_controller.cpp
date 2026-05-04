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
    ::write(sock, data.constData(), data.size());

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
    , m_mountPoint(QDir::homePath() + "/VaultMount")
    , m_lastActivity("—")
    , m_socketPath("/tmp/biometric_vault.sock")
    , m_fuseProcess(nullptr)
    , m_simProcess(nullptr)
    , m_pollTimer(nullptr)
    , m_selectedDevice("vault.img")
{
    /* Proje dizinini bul (gui'nin bir üst dizini) */
    m_projectDir = QCoreApplication::applicationDirPath();
    /* Build dizinindeyse, kaynak dizinine çık */
    if (m_projectDir.contains("/build")) {
        QDir d(m_projectDir);
        while (d.dirName() != "gui" && d.cdUp()) {}
        d.cdUp(); /* DigitalVault dizinine çık */
        m_projectDir = d.absolutePath();
    } else {
        QDir d(m_projectDir);
        d.cdUp(); /* gui -> DigitalVault */
        m_projectDir = d.absolutePath();
    }

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
    if (m_fuseProcess && m_fuseProcess->state() == QProcess::Running) {
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

/* ─── Vault Mount/Unmount ────────────────────────────────────────────────── */

bool VaultController::mountVault()
{
    if (m_fuseProcess && m_fuseProcess->state() == QProcess::Running) {
        return true; /* Zaten mount */
    }

    m_fuseProcess = new QProcess(this);
    connect(m_fuseProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &VaultController::onFuseFinished);

    QString fuseBin = QCoreApplication::applicationDirPath() + "/vault_fs";
    if (!QFile::exists(fuseBin)) {
        fuseBin = m_projectDir + "/fuse_vault/vault_fs";
    }

    if (!QFile::exists(fuseBin)) {
        emit notificationReceived("FUSE binary not found!", true);
        return false;
    }

    /* Mount dizinini oluştur */
    QDir().mkpath(m_mountPoint);

    /* Çalışma dizini olarak binary'nin olduğu yeri kullan (vault.img orada oluşsun) */
    m_fuseProcess->setWorkingDirectory(QFileInfo(fuseBin).absolutePath());
    
    printf("[GUI] Starting FUSE: %s -f %s %s\n", 
           fuseBin.toStdString().c_str(), 
           m_mountPoint.toStdString().c_str(),
           m_selectedDevice.toStdString().c_str());

    m_fuseProcess->start(fuseBin, QStringList() << "-f" << m_mountPoint << m_selectedDevice);

    /* FUSE'dan gelen audit loglarını dinle */
    connect(m_fuseProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        while (m_fuseProcess->canReadLine()) {
            QString line = QString::fromUtf8(m_fuseProcess->readLine()).trimmed();
            if (line.startsWith("[AUDIT]")) {
                QString auditMsg = line.mid(8).trimmed(); // "[AUDIT] " kısmını at
                QString level = "INFO";
                
                if (auditMsg.startsWith("UNLOCK")) level = "SUCCESS";
                else if (auditMsg.startsWith("DELETE")) level = "WARNING";
                else if (auditMsg.startsWith("ERROR")) level = "ERROR";
                
                addLog(level, auditMsg);
            }
        }
    });

    if (!m_fuseProcess->waitForStarted(5000)) {
        emit notificationReceived("Failed to start FUSE process!", true);
        return false;
    }

    return true;
}

bool VaultController::unmountVault()
{
    /* 1. Önce standart yöntemle unmount et (FUSE'a kapanma sinyali gider) */
    QProcess unmount;
    unmount.start("fusermount3", QStringList() << "-u" << m_mountPoint);
    if (!unmount.waitForFinished(2000)) {
        unmount.start("fusermount", QStringList() << "-u" << m_mountPoint);
        unmount.waitForFinished(2000);
    }

    /* 2. Eğer süreç hala çalışıyorsa (meşgulse), zorla durdur */
    if (m_fuseProcess && m_fuseProcess->state() == QProcess::Running) {
        /* Sürecin kendi kendine kapanması için biraz daha bekle */
        if (!m_fuseProcess->waitForFinished(2000)) {
            m_fuseProcess->terminate();
            if (!m_fuseProcess->waitForFinished(2000)) {
                m_fuseProcess->kill();
            }
        }
    }

    /* 3. Hala mount noktası kaldıysa temizle (Lazy unmount) */
    QProcess lazyUnmount;
    lazyUnmount.start("fusermount3", QStringList() << "-uz" << m_mountPoint);
    lazyUnmount.waitForFinished(1000);

    return true;
}

void VaultController::toggleVault()
{
    /* Çift tıklama veya hızlı tıklama koruması */
    if (m_fuseProcess && m_fuseProcess->state() == QProcess::Starting) {
        return;
    }

    if (m_isLocked) {
        /* Unlock — mount vault */
        if (!simulatorRunning()) {
            emit notificationReceived("Start the simulator first!", true);
            return;
        }

        addLog("INFO", "Starting unlock sequence for: " + m_selectedDevice);
        updateActivity("Mounting vault...");
        emit statusChanged();

        if (mountVault()) {
            m_isLocked = false;
            QString msg = "Vault unlocked and mounted at " + m_mountPoint;
            addLog("SUCCESS", "Vault accessible. Device: " + m_selectedDevice);
            updateActivity(msg);
            emit statusChanged();
            emit notificationReceived(msg, false);
        } else {
            addLog("ERROR", "Failed to mount vault on " + m_mountPoint);
            emit notificationReceived("Failed to mount vault!", true);
        }
    } else {
        /* Lock — unmount vault */
        addLog("INFO", "Locking vault and clearing memory keys...");
        updateActivity("Unmounting vault...");
        emit statusChanged();

        if (unmountVault()) {
            m_isLocked = true;
            updateActivity("Vault locked successfully.");
            emit statusChanged();
            emit notificationReceived("Vault locked and unmounted.", false);
        }
    }
}

void VaultController::onFuseFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_isLocked = true;

    /*
     * libfuse returns exit code 7 if the filesystem is unmounted forcefully
     * while it is busy (e.g. terminal is inside the directory).
     * This is expected behavior during a forced lock and is not a crash.
     */
    if (exitStatus == QProcess::NormalExit && exitCode == 7) {
        addLog("INFO", "Vault forcefully unmounted safely (was busy).");
        updateActivity("Vault unmounted (forced).");
    } else if (exitStatus == QProcess::CrashExit || exitCode != 0) {
        QString reason = (exitStatus == QProcess::CrashExit) ? "crashed" : "exited with error";
        addLog("ERROR", "Vault process " + reason + ". Exit code: " + QString::number(exitCode));
        updateActivity("Vault process " + reason + ".");
        emit notificationReceived("Vault process " + reason + " (code: " +
                                  QString::number(exitCode) + ")", true);
    } else {
        addLog("INFO", "Vault unmounted safely.");
        updateActivity("Vault unmounted.");
    }
    emit statusChanged();
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
    
    /* 1. Varsayılan dosya opsiyonunu ekle */
    QVariantMap fileOpt;
    fileOpt["name"] = "Dosya Konteynırı (vault.img)";
    fileOpt["path"] = "vault.img";
    fileOpt["description"] = "Proje klasöründe sanal bir dosya oluşturur.";
    devices << fileOpt;

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
                
                /* Sadece disk bölümlerini (part) veya diskleri ekle */
                if (type == "part" || type == "disk") {
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
    /* FUSE sürecinin durumunu kontrol et */
    bool wasLocked = m_isLocked;
    if (!m_isLocked && (!m_fuseProcess || m_fuseProcess->state() != QProcess::Running)) {
        m_isLocked = true;
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

    /* Dizin var mı kontrol et */
    QFileInfo info(localPath);
    QDir dir = info.dir();
    if (!dir.exists()) {
        emit notificationReceived("Target directory does not exist!", true);
        return false;
    }

    /* 
     * Gerçek oluşturma işlemini vault_fs'e bırakıyoruz.
     * Biz sadece yolu ayarlıyoruz ve kullanıcıya bilgi veriyoruz.
     */
    m_selectedDevice = localPath;
    updateActivity("New vault target set: " + info.fileName());
    emit statusChanged();
    emit notificationReceived("New vault path selected. It will be initialized on first unlock.", false);
    
    return true;
}

void VaultController::addLog(const QString &level, const QString &msg)
{
    QVariantMap log;
    log["ts"] = QDateTime::currentDateTime().toString("hh:mm:ss");
    log["level"] = level;
    log["msg"] = msg;
    
    m_securityLogs.prepend(log);
    if (m_securityLogs.size() > 100) m_securityLogs.removeLast();
    
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
