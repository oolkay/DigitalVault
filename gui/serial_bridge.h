#ifndef SERIAL_BRIDGE_H
#define SERIAL_BRIDGE_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QThread>
#include <atomic>

class SerialWorker;

/**
 * Single-flight binary UART bridge to STM32 / simulator.
 * All commands are serialized on a dedicated QThread.
 */
class SerialBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectionChanged)
    Q_PROPERTY(QString portName READ portName NOTIFY connectionChanged)
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)

public:
    explicit SerialBridge(QObject *parent = nullptr);
    ~SerialBridge() override;

    bool isConnected() const { return m_connected; }
    QString portName() const { return m_portName; }
    bool isBusy() const { return m_busy.load(std::memory_order_acquire); }

    Q_INVOKABLE void connectToPort(const QString &port);
    Q_INVOKABLE void disconnectPort();
    Q_INVOKABLE void startVerify();
    Q_INVOKABLE void requestKey();
    Q_INVOKABLE void startEnroll(int fingerId);
    Q_INVOKABLE void deleteFinger(int fingerId);
    Q_INVOKABLE void cancelOperation();
    Q_INVOKABLE void listEnrolled();
    Q_INVOKABLE void cancelInventoryIfRunning();

    bool tryStartVerify();
    bool tryStartEnroll(int fingerId);
    bool tryDeleteFinger(int fingerId);
    void forceIdle();

signals:
    void connectionChanged();
    void busyChanged();
    void operationRejected(const QString &reason);
    void sensorEvent(int eventCode);
    void verifyCompleted(bool success, int matchedId, const QString &error);
    void keyReceived(const QByteArray &key32);
    void enrollCompleted(bool success, int fingerId, const QString &error);
    void deleteCompleted(bool success, int fingerId, const QString &error);
    void listEnrolledCompleted(bool success, const QList<int> &fingerIds, const QString &error);
    void connectionLost(const QString &reason);

private:
    friend class SerialWorker;

    void setBusy(bool busy);

    QThread *m_thread = nullptr;
    SerialWorker *m_worker = nullptr;
    bool m_connected = false;
    std::atomic<bool> m_busy{false};
    QString m_portName;
};

#endif
