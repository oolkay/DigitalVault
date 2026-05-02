#ifndef VAULT_CONTROLLER_H
#define VAULT_CONTROLLER_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QDateTime>
#include <QProcess>
#include <QTimer>

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
    Q_INVOKABLE void        setSelectedDevice(const QString &path) { m_selectedDevice = path; }
    Q_INVOKABLE bool        createNewVault(const QString &path);

signals:
    void statusChanged();
    void fingerprintsChanged();
    void logsChanged();
    void notificationReceived(const QString &message, bool isError);

private slots:
    void onFuseFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onSimFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void pollStatus();

private:
    bool         m_isLocked;
    QVariantList m_fingerprints;
    QVariantList m_securityLogs;
    QString      m_mountPoint;
    QString      m_lastActivity;
    QString      m_socketPath;
    QString      m_projectDir;
    QString      m_selectedDevice;

    QProcess    *m_fuseProcess;
    QProcess    *m_simProcess;
    QTimer      *m_pollTimer;

    void updateActivity(const QString &msg);
    bool mountVault();
    bool unmountVault();
    void loadFingerprintsFromHAL();
};

#endif // VAULT_CONTROLLER_H
