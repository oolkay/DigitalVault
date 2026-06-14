#ifndef VAULT_CONTROLLER_H
#define VAULT_CONTROLLER_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QDateTime>
#include <QProcess>
#include <QTimer>
#include <QFutureWatcher>
#include <QSet>

#include <atomic>
#include <functional>

#include <QtQml/qqmlregistration.h>

class SerialBridge;
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
    Q_PROPERTY(bool fingerprintUnlockActive READ fingerprintUnlockActive NOTIFY fingerprintUnlockActiveChanged)
    Q_PROPERTY(QString fingerprintUnlockMessage READ fingerprintUnlockMessage NOTIFY fingerprintUnlockActiveChanged)
    Q_PROPERTY(QString fingerprintFlow READ fingerprintFlow NOTIFY fingerprintUnlockActiveChanged)
    Q_PROPERTY(int fingerprintPromptState READ fingerprintPromptState NOTIFY fingerprintPromptStateChanged)
    Q_PROPERTY(bool fingerprintAwaitingDecision READ fingerprintAwaitingDecision NOTIFY fingerprintPromptStateChanged)
    Q_PROPERTY(int fingerprintEnrollStep READ fingerprintEnrollStep NOTIFY fingerprintEnrollStepChanged)
    Q_PROPERTY(int setupOwnerFingerId READ setupOwnerFingerId NOTIFY setupOwnerFingerIdChanged)
    Q_PROPERTY(QString setupOwnerLabel READ setupOwnerLabel WRITE setSetupOwnerLabel NOTIFY setupOwnerLabelChanged)
    Q_PROPERTY(QVariantList setupAccessEntries READ setupAccessEntries NOTIFY setupAccessEntriesChanged)
    Q_PROPERTY(QObject *serialBridge READ serialBridgeObject CONSTANT)
    Q_PROPERTY(bool sensorConnected READ sensorConnected NOTIFY statusChanged)
    Q_PROPERTY(QString sensorStatus READ sensorStatus NOTIFY sensorStatusChanged)
    Q_PROPERTY(QString sensorStatusText READ sensorStatusText NOTIFY sensorStatusChanged)
    Q_PROPERTY(QString sensorPortPath READ sensorPortPath NOTIFY sensorStatusChanged)
    Q_PROPERTY(bool autoDetectUsb READ autoDetectUsb WRITE setAutoDetectUsb NOTIFY autoDetectUsbChanged)
    Q_PROPERTY(bool hardwareDisconnectLockdown READ hardwareDisconnectLockdown WRITE setHardwareDisconnectLockdown NOTIFY hardwareDisconnectLockdownChanged)
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
    Q_INVOKABLE void        forceLockVault();
    Q_INVOKABLE void        beginFingerprintUnlock();
    Q_INVOKABLE void        cancelFingerprintUnlock();
    Q_INVOKABLE void        continueAfterDecision();
    Q_INVOKABLE void        beginSetupOwnerEnroll(int fingerId);
    Q_INVOKABLE void        beginSetupOwnerEnrollForDevice(const QString &devicePath);
    Q_INVOKABLE void        prepareSetupOwnerSlot(const QString &devicePath);
    Q_INVOKABLE void        clearSetupWizardSession();
    Q_INVOKABLE int         ownerFingerSlotForDevice(const QString &devicePath) const;
    Q_INVOKABLE int         nextFreeFingerSlot(const QString &excludeDevicePath = QString()) const;
    Q_INVOKABLE bool        beginVaultSetupBiometric(const QString &device, bool force, int vaultSizeMB,
                                                     const QString &vaultName, const QString &normalName,
                                                     const QString &normalFs);
    Q_INVOKABLE bool        fingerInAccessList(int fingerId) const;
    Q_INVOKABLE QVariantList accessListForDevice(const QString &devicePath) const;
    Q_INVOKABLE void        addFingerprint();
    Q_INVOKABLE void        removeFingerprint(int index);
    Q_INVOKABLE void        renameFingerprint(int index, const QString &newName);
    Q_INVOKABLE QVariantList getStorageDevices();
    Q_INVOKABLE QString     fingerprintLabel(int index) const;
    Q_INVOKABLE void        refreshFingerprints();
    Q_INVOKABLE void        startSimulator();
    Q_INVOKABLE void        stopSimulator();
    Q_INVOKABLE void        reconnectSerial();
    Q_INVOKABLE void        connectToConfiguredPort(const QString &port);
    Q_INVOKABLE QStringList availableSerialPorts();
    Q_INVOKABLE void        clearSerialPortSettings();
    Q_INVOKABLE void        setSelectedDevice(const QString &path);
    Q_INVOKABLE QVariantMap createSetupSession(const QString &devicePath = QString()) const;
    Q_INVOKABLE QString     defaultContainersDir() const;
    Q_INVOKABLE bool        createLoopDevice(const QString &containerName, int totalSizeMB,
                                             const QString &storageDir = QString());
    Q_INVOKABLE bool        deleteDevice(const QString &devicePath);
    Q_INVOKABLE bool        deleteVault(const QString &devicePath);
    Q_INVOKABLE bool        deleteVaultRequiresBiometric(const QString &devicePath) const;
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
    Q_INVOKABLE void        navigateToVaultManagerDetail(const QString &devicePath);
    Q_INVOKABLE QString     takePendingVaultManagerDetailPath();
    
    int  autoLockTimeout() const { return m_autoLockTimeout; }
    void setAutoLockTimeout(int minutes);
    
    bool isToggling() const { return m_isToggling; }
    bool fingerprintUnlockActive() const { return m_fpUnlockActive; }
    QString fingerprintUnlockMessage() const { return m_fpUnlockMessage; }
    int fingerprintPromptState() const { return m_fpPromptState; }
    bool fingerprintAwaitingDecision() const { return m_fpAwaitingDecision; }
    int fingerprintEnrollStep() const { return m_fpEnrollStep; }
    QString fingerprintFlow() const;
    int setupOwnerFingerId() const { return m_setupOwnerFingerId; }
    QString setupOwnerLabel() const { return m_setupOwnerLabel; }
    void setSetupOwnerLabel(const QString &label);
    QVariantList setupAccessEntries() const { return m_pendingSetupAccessEntries; }
    QObject *serialBridgeObject() const;
    SerialBridge *serialBridge() const { return m_serialBridge; }
    bool sensorConnected() const;
    QString sensorStatus() const { return m_sensorStatus; }
    QString sensorStatusText() const { return m_sensorStatusText; }
    QString sensorPortPath() const { return m_sensorPortPathDisplay; }
    bool autoDetectUsb() const;
    void setAutoDetectUsb(bool enabled);
    bool hardwareDisconnectLockdown() const;
    void setHardwareDisconnectLockdown(bool enabled);
    Q_INVOKABLE QString savedSerialPort() const;

    /* ── Multi-user Access Management ── */
    Q_INVOKABLE bool        verifyOwner(int fingerId);
    Q_INVOKABLE bool        grantAccess(int ownerFingerId, int newFingerId);
    Q_INVOKABLE void        beginGrantAccessEnroll(int ownerFingerId, int newFingerId, const QString &label);
    Q_INVOKABLE void        beginGrantAccessForVault(const QString &devicePath, const QString &label);
    Q_INVOKABLE void        beginRevokeAccessForVault(const QString &devicePath, int fingerId);
    Q_INVOKABLE void        beginSetupAccessEnroll(const QString &label);
    Q_INVOKABLE bool        removePendingSetupAccess(int fingerId);
    Q_INVOKABLE void        clearPendingSetupAccess();
    Q_INVOKABLE void        beginDeleteVaultBiometricGate(const QString &devicePath);
    Q_INVOKABLE bool        revokeAccess(int ownerFingerId, int slotNumber);
    Q_INVOKABLE bool        isFingerAllowedForVault(const QString &devicePath, int fingerId) const;
    Q_INVOKABLE bool        labelExistsForVault(const QString &devicePath, const QString &label) const;
    Q_INVOKABLE bool        renameVaultFingerprintLabel(const QString &devicePath, int fingerId,
                                                        const QString &newLabel);
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
    void fingerprintUnlockActiveChanged();
    void fingerprintPromptStateChanged();
    void fingerprintEnrollStepChanged();
    void setupOwnerFingerIdChanged();
    void setupOwnerLabelChanged();
    void setupAccessEntriesChanged();
    void deleteVaultGatePassed(const QString &devicePath);
    void vaultAccessListChanged();
    void autoLockTimeoutChanged();
    void navigationRequested(const QString &pageFile);
    void sensorStatusChanged();
    void autoDetectUsbChanged();
    void hardwareDisconnectLockdownChanged();
    void lockRequiresForceConfirmation(const QString &devicePath);

private slots:
    void onSimFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onSerialConnectionChanged();
    void pollStatus();

private:
    enum class FpFlow {
        None,
        Unlock,
        SetupFindExisting,
        SetupEnroll,
        SetupVerify,
        SetupAccessFindExisting,
        SetupAccessEnroll,
        GrantVerify,
        GrantFindExisting,
        GrantEnroll,
        RevokeVerify,
        DeleteVerify
    };

    void resetAutoLockTimer();
    void handleAutoLock();
    void beginLockVault(bool force = false);
    void beginUnlockVault();
    void finishUnlockWithKey(const QByteArray &key32);
    void finishSetupWithKey(const QByteArray &key32);
    bool isFingerAllowedForSelectedVault(int fingerId) const;
    void handleVerifyCompleted(bool ok, int matchedId, const QString &err);
    void handleSensorEvent(int eventCode);
    void startAuthorizedVerify(FpFlow flow, const QString &devicePath, const QString &initialMessage);
    void startGrantEnrollForVault(const QString &devicePath, const QString &label);
    void finishRevokeAfterGate(const QString &devicePath, int fingerId);
    bool appendAccessListEntry(const QString &devicePath, int fingerId, const QString &label, bool isOwner);
    void startGrantFindExistingForVault(const QString &devicePath, const QString &label);
    void clearFingerprintFlowState();
    void beginFingerprintOperationUi(FpFlow flow, const QString &message);
    void abortFingerprintOperationUi();
    void endFingerprintOperationUi();
    void setFingerprintPromptOutcome(int state, const QString &message);
    static bool isLiveFingerprintFlow(FpFlow flow);
    bool ensureFingerprintIdle();
    void cleanupGrantEnrollOrphan();
    void cleanupSetupOwnerOrphan();
    int runVaultManagerWithKey(const QStringList &args, const QByteArray &key32, int timeoutMs);
    QString m_lastVaultManagerOutput;
    void launchSetupAsync(const QString &device, int ownerId, bool force, int vaultSizeMB,
                          const QString &vaultName, const QString &normalName, const QString &normalFs,
                          const QString &ownerLabel, const QByteArray &key32);
    enum class SensorStatus { Unconfigured, Disconnected, Connecting, Connected, Error };

    QString resolveSerialPort();
    void updateSensorStatus(SensorStatus status, const QString &customText = QString());
    void onSerialConnectionLost(const QString &reason);
    void triggerHardwareDisconnectLockdown(const QString &reason);
    void triggerStorageDisconnectLockdown(const QString &reason);
    void endStartupConnectPhase();
    void clearFailedSerialPortSetting();
    void tryConnectSerial();
    void scheduleSerialConnectRetries();
    void beginSetupOwnerEnrollAfterConnect(int fingerId);
    void beginSetupOwnerEnrollForDeviceAfterConnect(const QString &devicePath);
    void beginFingerprintUnlockAfterConnect();
    void beginOwnerEnrollAuto();
    void beginSetupFindExisting();
    void beginSetupEnrollAfterNoMatch();
    void beginSetupAccessEnrollAfterNoMatch();
    bool appendPendingSetupAccess(int fingerId, const QString &label, bool newlyEnrolled);
    bool pendingSetupAccessContainsFinger(int fingerId) const;
    bool pendingSetupAccessLabelExists(const QString &label) const;
    void rollbackPendingSetupAccess();
    void cleanupNextPendingSetupAccessSlot();
    void onDeleteCompletedForEnroll(bool ok, int fingerId, const QString &err);
    void onSerialBusyWatchdog();
    QSet<int> collectUsedFingerSlots(const QString &excludeDevicePath) const;
    QSet<int> collectSensorOccupiedSlots() const;
    bool isSlotReferencedInRegistry(int fingerId) const;
    bool isOrphanSensorSlot(int fingerId) const;
    void loadSensorSlotCache();
    void saveSensorSlotCache();
    void markSensorSlot(int fingerId, bool occupied);
    void refreshSensorInventory();
    void scheduleSensorInventoryRefresh();
    void performSensorInventoryRefresh();
    void runExclusiveSerialOp(const std::function<void()> &action, bool preemptInventory = true);
    void runExclusiveSerialOpRetry(int generation, const std::function<void()> &action,
                                   bool preemptInventory, int elapsedMs);
    void rollbackSetupEnrollAfterVerifyFail(const QString &reason);
    void onListEnrolledCompleted(bool ok, const QList<int> &fingerIds, const QString &err);
    void startVaultSetupVerify();
    void requestVaultSetupKeyAfterEnroll();
    enum class LockResult {
        Success,
        Busy,
        Failed
    };

    FpFlow m_fpFlow = FpFlow::None;
    QString m_fpContextDevice;
    int m_setupOwnerFingerId = -1;
    QString m_setupOwnerLabel;
    QString m_setupDevicePath;
    QVariantList m_pendingSetupAccessEntries;
    QSet<int> m_pendingSetupNewSlots;
    QList<int> m_pendingSetupCleanupSlots;
    QString m_pendingSetupAccessLabel;
    bool m_pendingSetupAccessCleanupRunning = false;
    int m_pendingEnrollSlot = -1;
    bool m_pendingDeleteBeforeEnroll = false;
    int m_pendingSetupVerifyRollback = -1;
    bool m_pendingEnrollRetryAfterExists = false;
    bool m_setupOwnerEnrolledThisSession = false;
    qint64 m_setupEnrollKeyGraceUntilMs = 0;
    bool m_pendingSetupGetKeyOnly = false;
    mutable QSet<int> m_sensorOccupiedSlots;
    int m_pendingGrantOwnerId = -1;
    int m_pendingGrantFingerId = -1;
    QString m_pendingGrantLabel;
    int m_pendingRevokeFingerId = -1;
    QString m_pendingDeleteDevicePath;
    bool m_pendingRegistryRollbackSlot = false;
    struct PendingSetup {
        QString device;
        bool force = false;
        int vaultSizeMB = 0;
        QString vaultName;
        QString normalName;
        QString normalFs;
        QString ownerLabel;
        bool valid = false;
    } m_pendingSetup;

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
    QString      m_pendingVaultManagerDetailPath;
    QByteArray   m_vaultManagerBinCache;
    QByteArray   m_mountPointCache;

    QProcess    *m_simProcess;
    QTimer      *m_pollTimer;
    QFutureWatcher<int> *m_setupWatcher;
    SerialBridge *m_serialBridge = nullptr;
    bool m_fpUnlockActive = false;
    QString m_fpUnlockMessage;
    int m_fpPromptState = 0;
    bool m_fpAwaitingDecision = false;
    int m_fpDecisionMatchedId = -1;
    int m_fpEnrollStep = 0;
    QProcess *m_socatProcess = nullptr;
    int m_serialRetryGeneration = 0;
    QString m_sensorStatus = QStringLiteral("unconfigured");
    QString m_sensorStatusText;
    QString m_sensorPortPathDisplay;
    QString m_lastAttemptedPort;
    bool m_startupConnectPhase = true;
    bool m_manualConnectRequested = false;
    QTimer *m_inventoryRefreshTimer = nullptr;
    QTimer *m_serialBusyWatchdog = nullptr;
    int m_serialExclusiveGeneration = 0;
    bool m_serialConnectQueued = false;

    void updateActivity(const QString &msg);
    bool mountVault();
    LockResult unmountVault(bool force);
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
    bool isBlockDevicePresent(const QString &path) const;
    bool unlockedVaultBackingPresent() const;
    void syncRuntimeFromKernel();
    QString findLuksPartitionPath(const QString &devicePath) const;
    bool isRegistryManagedPath(const QString &path) const;
    void saveState(const QString &lastKnownState, bool setupInProgress);
    QJsonObject loadState() const;
    bool removeRegistryEntry(const QString &devicePath, QString *removedBacking = nullptr);
    static std::atomic<bool> s_emergencyClosing;
};

#endif // VAULT_CONTROLLER_H
