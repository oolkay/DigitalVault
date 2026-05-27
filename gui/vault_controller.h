#ifndef VAULT_CONTROLLER_H
#define VAULT_CONTROLLER_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QDateTime>
#include <QProcess>
#include <QTimer>
#include <QFutureWatcher>

#include <atomic>

#include <QtQml/qqmlregistration.h>

class VaultController;
extern VaultController *g_vaultController;

class VaultController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool isLocked          READ isLocked          NOTIFY statusChanged)
    Q_PROPERTY(QString vaultStatusText READ vaultStatusText   NOTIFY statusChanged)
    Q_PROPERTY(int fingerprintCount   READ fingerprintCount   NOTIFY fingerprintsChanged)
    Q_PROPERTY(bool simulatorRunning  READ simulatorRunning   NOTIFY statusChanged)
    Q_PROPERTY(QString selectedDevice READ selectedDevice    NOTIFY statusChanged)
    Q_PROPERTY(QString unlockedDevice READ unlockedDevice    NOTIFY statusChanged)
    Q_PROPERTY(QVariantList securityLogs READ securityLogs    NOTIFY logsChanged)
    Q_PROPERTY(bool isSetupRunning    READ isSetupRunning     NOTIFY setupRunningChanged)
    Q_PROPERTY(bool isToggling        READ isToggling        NOTIFY togglingChanged)
    Q_PROPERTY(int autoLockTimeout    READ autoLockTimeout    WRITE setAutoLockTimeout NOTIFY autoLockTimeoutChanged)
    Q_PROPERTY(QString setupStatusText READ setupStatusText   NOTIFY setupRunningChanged)
    Q_PROPERTY(bool isSelectedDeviceVault READ isSelectedDeviceVault NOTIFY statusChanged)
    Q_PROPERTY(int selectedDeviceCapacityMB READ selectedDeviceCapacityMB NOTIFY statusChanged)
    Q_PROPERTY(QVariantMap selectedDeviceInfo READ selectedDeviceInfo NOTIFY statusChanged)
    Q_PROPERTY(QVariantList storageDevices READ storageDevices NOTIFY devicesChanged)
    Q_PROPERTY(QVariantList registeredVaults READ registeredVaults NOTIFY devicesChanged)
    Q_PROPERTY(QVariantList managedDevices READ managedDevices NOTIFY devicesChanged)
    Q_PROPERTY(QVariantList discoveredStorageDevices READ discoveredStorageDevices NOTIFY devicesChanged)

public:
    explicit VaultController(QObject *parent = nullptr);
    ~VaultController();

    bool    isLocked()        const { return m_isLocked; }
    QString vaultStatusText() const { return m_isLocked ? "Vault Secured" : "Vault Accessible"; }
    int     fingerprintCount()const { return m_fingerprints.size(); }
    QString mountPoint()      const { return m_mountPoint; }
    QString lastActivity()    const { return m_lastActivity; }
    QString selectedDevice()  const { return m_selectedDevice; }
    QString unlockedDevice()  const { return m_unlockedDevice; }
    QVariantList securityLogs() const { return m_securityLogs; }
    bool    isSetupRunning()  const { return m_isSetupRunning; }
    QString setupStatusText() const { return m_setupStatusText; }
    bool    isSelectedDeviceVault() const { return m_isSelectedDeviceVault; }
    int     selectedDeviceCapacityMB() const { return m_selectedDeviceCapacityMB; }
    QVariantMap selectedDeviceInfo() const { return m_selectedDeviceInfo; }
    QVariantList storageDevices() const { return m_storageDevices; }
    QVariantList registeredVaults() const;
    QVariantList managedDevices() const;
    QVariantList discoveredStorageDevices() const;

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
    Q_INVOKABLE void        setSelectedDevice(const QString &path);
    Q_INVOKABLE QVariantMap createSetupSession(const QString &devicePath = QString()) const;
    Q_INVOKABLE QString     defaultContainersDir() const;
    Q_INVOKABLE bool        createLoopDevice(const QString &containerName, int totalSizeMB,
                                             const QString &storageDir = QString());
    Q_INVOKABLE bool        deleteDevice(const QString &devicePath);
    Q_INVOKABLE bool        deleteVault(const QString &devicePath);
    Q_INVOKABLE QVariantMap previewDeleteVault(const QString &devicePath) const;
    Q_INVOKABLE bool        renameVaultDisplayName(const QString &devicePath, const QString &newName);
    Q_INVOKABLE bool        reconcileVaultRuntime();
    Q_INVOKABLE QVariantMap storageLayoutForDevice(const QString &diskPath) const;
    Q_INVOKABLE QVariantList availablePartitionActions(const QString &diskPath,
                                                       const QString &partPath = QString()) const;
    Q_INVOKABLE QVariantMap buildStoragePlan(const QString &actionId,
                                             const QVariantMap &params) const;
    Q_INVOKABLE QVariantMap previewStoragePlan(const QVariantMap &plan) const;
    Q_INVOKABLE bool        applyStoragePlan(const QVariantMap &plan);
    Q_INVOKABLE QStringList supportedFilesystems() const;
    Q_INVOKABLE QString     resolveManagedDiskPath(const QString &registryPath) const;
    Q_INVOKABLE QString     registryBackingForDevice(const QString &devicePath) const;
    Q_INVOKABLE bool        emergencyClose();
    Q_INVOKABLE bool        vaultRuntimeOpen() const;
    const char *cachedVaultManagerBin() const;
    const char *cachedMountPoint() const;
    static int posixEmergencyClose(const char *vaultManagerBin, const char *mountPoint);
    Q_INVOKABLE bool        setupVault(const QString &device, bool force, int ownerId, int vaultSizeMB, 
                                       const QString &vaultName, const QString &normalName, const QString &normalFs);
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
    Q_INVOKABLE QVariantList getKeyslotsForDevice(const QString &devicePath);
    Q_INVOKABLE QVariantMap vaultDetailForDevice(const QString &devicePath) const;
    Q_INVOKABLE bool isVaultUnlockedForDevice(const QString &devicePath) const;
    Q_INVOKABLE QVariantList getSecurityLogsPage(int page, int pageSize);
    Q_INVOKABLE void        clearLogs();
    Q_INVOKABLE void        notifyUser(const QString &message, bool isError = false);
    Q_INVOKABLE void        navigateToPage(const QString &pageFile);

signals:
    void statusChanged();
    void fingerprintsChanged();
    void logsChanged();
    void notificationReceived(const QString &message, bool isError);
    void setupRunningChanged();
    void setupFinished(bool success, QString message);
    void loopCreateFinished(bool success, QString message);
    void deviceDeleteFinished(bool success, QString message);
    void storagePlanFinished(bool success, QString message);
    void devicesChanged();
    void togglingChanged();
    void autoLockTimeoutChanged();
    void navigationRequested(const QString &pageFile);

private slots:
    void onSimFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void pollStatus();

private:
    void resetAutoLockTimer();
    void handleAutoLock();
    void beginLockVault();
    void beginUnlockVault();

    bool         m_isLocked;
    int          m_autoLockTimeout; // minutes, 0 = never
    QTimer      *m_autoLockTimer;
    bool         m_isSetupRunning;
    bool         m_isToggling;
    QString      m_setupStatusText;
    QVariantList m_fingerprints;
    QVariantList m_securityLogs;
    QVariantList m_storageDevices;
    QString      m_mountPoint;
    QString      m_lastActivity;
    QString      m_socketPath;
    QString      m_projectDir;
    QString      m_selectedDevice;
    QString      m_unlockedDevice;
    bool         m_isSelectedDeviceVault;
    int          m_selectedDeviceCapacityMB;
    QVariantMap  m_selectedDeviceInfo;
    QString      m_vaultManagerBin;
    QString      m_logFilePath;
    QString      m_registryPath;
    QString      m_containersDir;
    QString      m_statePath;
    QByteArray   m_vaultManagerBinCache;
    QByteArray   m_mountPointCache;

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

    QString sanitizeContainerName(const QString &name) const;
    QJsonArray loadRegistryArray() const;
    bool saveRegistryEntry(const QJsonObject &entry);
    void syncRegistryWithLosetup();
    bool isRegistryLoopDevice(const QString &devPath) const;
    QString registryLabelForDevice(const QString &devPath) const;
    QJsonObject registryEntryForDevice(const QString &devPath) const;
    QVariantMap resolveDeviceInfo(const QString &path) const;
    void applySelectedDeviceInfo(const QVariantMap &info);
    void selectDeviceByPath(const QString &path, bool forceRefresh = false);
    void refreshStorageDevices();
    static bool deviceListsEqual(const QVariantList &a, const QVariantList &b);
    QVariantMap enrichVaultEntry(const QVariantMap &entry) const;
    QVariantList fetchKeyslotsForDevice(const QString &devicePath) const;

    void startupRecovery();
    bool isVaultRuntimeOpen() const;
    bool isMapperPresent() const;
    bool isUserVaultMounted() const;
    void syncRuntimeFromKernel();
    QString findLuksPartitionPath(const QString &devicePath) const;
    bool isRegistryManagedPath(const QString &path) const;
    void saveState(const QString &lastKnownState, bool setupInProgress);
    QJsonObject loadState() const;
    bool removeRegistryEntry(const QString &devicePath, QString *removedBacking = nullptr);
    static std::atomic<bool> s_emergencyClosing;
};

#endif // VAULT_CONTROLLER_H
