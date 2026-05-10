#ifndef VAULT_CONTROLLER_H
#define VAULT_CONTROLLER_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QDateTime>
#include <QProcess>
#include <QTimer>
#include <QFutureWatcher>

#include <QtQml/qqmlregistration.h>

class VaultController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool isLocked          READ isLocked          NOTIFY statusChanged)
    Q_PROPERTY(QString vaultStatusText READ vaultStatusText   NOTIFY statusChanged)
    Q_PROPERTY(int fingerprintCount   READ fingerprintCount   NOTIFY fingerprintsChanged)
    Q_PROPERTY(bool simulatorRunning  READ simulatorRunning   NOTIFY statusChanged)
    Q_PROPERTY(QString selectedDevice READ selectedDevice    NOTIFY statusChanged)
    Q_PROPERTY(QVariantList securityLogs READ securityLogs    NOTIFY logsChanged)
    Q_PROPERTY(bool isSetupRunning    READ isSetupRunning     NOTIFY setupRunningChanged)
    Q_PROPERTY(bool isToggling        READ isToggling        NOTIFY togglingChanged)
    Q_PROPERTY(int autoLockTimeout    READ autoLockTimeout    WRITE setAutoLockTimeout NOTIFY autoLockTimeoutChanged)
    Q_PROPERTY(QString setupStatusText READ setupStatusText   NOTIFY setupRunningChanged)

public:
    explicit VaultController(QObject *parent = nullptr);
    ~VaultController();

    bool    isLocked()        const { return m_isLocked; }
    QString vaultStatusText() const { return m_isLocked ? "Vault Secured" : "Vault Accessible"; }
    int     fingerprintCount()const { return m_fingerprints.size(); }
    QString mountPoint()      const { return m_mountPoint; }
    QString lastActivity()    const { return m_lastActivity; }
    QString selectedDevice()  const { return m_selectedDevice; }
    QVariantList securityLogs() const { return m_securityLogs; }
    bool    isSetupRunning()  const { return m_isSetupRunning; }
    QString setupStatusText() const { return m_setupStatusText; }

    void    addLog(const QString &level, const QString &msg);
    bool    simulatorRunning() const;

    Q_INVOKABLE void        toggleVault();
    Q_INVOKABLE void        addFingerprint();
    Q_INVOKABLE void        removeFingerprint(int index);
    Q_INVOKABLE void        renameFingerprint(int index, const QString &newName);
    Q_INVOKABLE QVariantList getStorageDevices();
    Q_INVOKABLE QString     fingerprintLabel(int index) const;
    Q_INVOKABLE void        refreshFingerprints();
    Q_INVOKABLE void        startSimulator();
    Q_INVOKABLE void        stopSimulator();
    Q_INVOKABLE void        setSelectedDevice(const QString &path) { m_selectedDevice = path; emit statusChanged(); }
    Q_INVOKABLE bool        createNewVault(const QString &path);
    Q_INVOKABLE bool        setupVault(const QString &device, bool force = false, int ownerId = 1);
    Q_INVOKABLE void        openInFileManager();
    Q_INVOKABLE void        openInTerminal();
    
    int  autoLockTimeout() const { return m_autoLockTimeout; }
    void setAutoLockTimeout(int minutes);
    
    bool isToggling() const { return m_isToggling; }

    /* ── Multi-user Access Management ── */
    Q_INVOKABLE bool        verifyOwner(int fingerId);
    Q_INVOKABLE bool        grantAccess(int ownerFingerId, int newFingerId);
    Q_INVOKABLE bool        revokeAccess(int ownerFingerId, int slotNumber);
    Q_INVOKABLE QVariantList getKeyslots();
    Q_INVOKABLE QVariantList getSecurityLogsPage(int page, int pageSize);
    Q_INVOKABLE void        clearLogs();

signals:
    void statusChanged();
    void fingerprintsChanged();
    void logsChanged();
    void notificationReceived(const QString &message, bool isError);
    void setupRunningChanged();
    void setupFinished(bool success, QString message);
    void togglingChanged();
    void autoLockTimeoutChanged();

private slots:
    void onSimFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void pollStatus();

private:
    void resetAutoLockTimer();
    void handleAutoLock();

    bool         m_isLocked;
    int          m_autoLockTimeout; // minutes, 0 = never
    QTimer      *m_autoLockTimer;
    bool         m_isSetupRunning;
    bool         m_isToggling;
    QString      m_setupStatusText;
    QVariantList m_fingerprints;
    QVariantList m_securityLogs;
    QString      m_mountPoint;
    QString      m_lastActivity;
    QString      m_socketPath;
    QString      m_projectDir;
    QString      m_selectedDevice;
    QString      m_vaultManagerBin;
    QString      m_logFilePath;

    QProcess    *m_simProcess;
    QTimer      *m_pollTimer;
    QFutureWatcher<int> *m_setupWatcher;

    void updateActivity(const QString &msg);
    bool mountVault();
    bool unmountVault();
    void loadFingerprintsFromHAL();

    /* vault_manager binary'sinin yolunu bul */
    QString findVaultManager() const;

    /* vault_manager komutunu pkexec (veya sudo fallback) ile çalıştır */
    int runVaultManager(const QStringList &args, int timeoutMs = 15000);
};

#endif // VAULT_CONTROLLER_H
