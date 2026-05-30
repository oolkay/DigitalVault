#include "serial_bridge.h"

#include <QMetaObject>
#include <QSocketNotifier>
#include <QSettings>
#include <QTimer>
#include <QDebug>
#include <QThread>
#include <QFile>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

namespace {

constexpr quint8 kSync = 0xAA;
constexpr quint8 kCmdEnroll = 0x01;
constexpr quint8 kCmdVerify = 0x02;
constexpr quint8 kCmdDelete = 0x03;
constexpr quint8 kCmdCancel = 0x04;
constexpr quint8 kCmdGetKey = 0x05;
constexpr quint8 kCmdListEnrolled = 0x06;

constexpr quint8 kEvtMin = 0x10;
constexpr quint8 kEvtMax = 0x14;
constexpr quint8 kResSuccess = 0x20;
constexpr quint8 kResFailNoFinger = 0x21;
constexpr quint8 kResFailBadImage = 0x22;
constexpr quint8 kResNoMatch = 0x23;
constexpr quint8 kResFailAlreadyExists = 0x24;
constexpr quint8 kResFailSlotFull = 0x25;
constexpr quint8 kResFailInvalidId = 0x26;
constexpr quint8 kResCancelled = 0x27;
constexpr quint8 kResTimeout = 0x28;
constexpr quint8 kResKey = 0x29;
constexpr quint8 kResBusy = 0x2A;
constexpr quint8 kResSensorError = 0x2B;

QString enrollErrorMessage(quint8 code, const QByteArray &payload)
{
    const int existingId = payload.isEmpty() ? -1 : static_cast<quint8>(payload.at(0));
    switch (code) {
    case kResFailNoFinger:
        return QStringLiteral("Sensörde parmak algılanmadı");
    case kResFailBadImage:
        return QStringLiteral("Parmak izi okunamadı — tekrar deneyin");
    case kResNoMatch:
        return QStringLiteral("Parmak izi eşleşmedi");
    case kResFailAlreadyExists:
        if (existingId >= 0)
            return QStringLiteral("Sensor slot #%1 is already enrolled").arg(existingId);
        return QStringLiteral("Sensor slot is already enrolled");
    case kResFailSlotFull:
        return QStringLiteral("Sensor storage full (max 127 templates)");
    case kResFailInvalidId:
        return QStringLiteral("Invalid fingerprint slot ID (use 1–127)");
    case kResCancelled:
        return QStringLiteral("İşlem iptal edildi");
    case kResTimeout:
        return QStringLiteral("Sensör zaman aşımı — tekrar deneyin");
    case kResBusy:
        return QStringLiteral("Sensör meşgul — lütfen bekleyin");
    case kResSensorError:
        return QStringLiteral("Sensör yanıt vermiyor — bağlantıyı kontrol edin");
    default:
        return QStringLiteral("Enroll failed (sensor code 0x%1)").arg(code, 2, 16, QChar('0'));
    }
}

QString verifyErrorMessage(quint8 code)
{
    switch (code) {
    case kResFailNoFinger:
        return QStringLiteral("Sensörde parmak algılanmadı");
    case kResFailBadImage:
        return QStringLiteral("Parmak izi okunamadı — tekrar deneyin");
    case kResNoMatch:
        return QStringLiteral("Parmak izi eşleşmedi");
    case kResCancelled:
        return QStringLiteral("Doğrulama iptal edildi");
    case kResTimeout:
        return QStringLiteral("Doğrulama zaman aşımı — tekrar deneyin");
    case kResBusy:
        return QStringLiteral("Sensör meşgul — lütfen bekleyin");
    case kResSensorError:
        return QStringLiteral("Sensör yanıt vermiyor — bağlantıyı kontrol edin");
    default:
        return QStringLiteral("Doğrulama başarısız (kod 0x%1)").arg(code, 2, 16, QChar('0'));
    }
}

constexpr int kVerifyTimeoutMs = 15000;
constexpr int kEnrollTimeoutMs = 60000;
constexpr int kPostCancelMs = 2000;

quint8 xorChecksum(const QByteArray &body)
{
    quint8 x = 0;
    for (char c : body)
        x ^= static_cast<quint8>(c);
    return x;
}

QByteArray buildPacket(quint8 code, const QByteArray &payload = {})
{
    QByteArray body;
    body.append(static_cast<char>(kSync));
    body.append(static_cast<char>(code));
    body.append(static_cast<char>(payload.size()));
    body.append(payload);
    body.append(static_cast<char>(xorChecksum(body)));
    return body;
}

int configurePort(int fd)
{
    struct termios tio;
    if (tcgetattr(fd, &tio) < 0)
        return -1;
    cfmakeraw(&tio);
    /* PTY (socat) ignores baud; cfsetispeed fails on Linux pseudo-ttys */
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 1;
    return tcsetattr(fd, TCSANOW, &tio);
}

} // namespace

void SerialBridge::setBusy(bool busy)
{
    const bool prev = m_busy.exchange(busy, std::memory_order_acq_rel);
    if (prev != busy)
        emit busyChanged();
}

class SerialWorker : public QObject
{
    Q_OBJECT
public:
    explicit SerialWorker(SerialBridge *bridge)
        : m_bridge(bridge)
    {
    }

public slots:
    void setupOnWorkerThread()
    {
        Q_ASSERT(thread() == QThread::currentThread());

        m_timer = new QTimer(this);
        m_recoveryTimer = new QTimer(this);
        m_timer->setSingleShot(true);
        m_recoveryTimer->setSingleShot(true);
        connect(m_timer, &QTimer::timeout, this, &SerialWorker::onDeadline);
        connect(m_recoveryTimer, &QTimer::timeout, this, &SerialWorker::onRecoveryDone);

        m_linkWatch = new QTimer(this);
        m_linkWatch->setInterval(750);
        connect(m_linkWatch, &QTimer::timeout, this, &SerialWorker::checkLink);
    }

    void connectPort(const QString &port)
    {
        if (m_fd >= 0) {
            if (m_notifier) {
                m_notifier->setEnabled(false);
                delete m_notifier;
                m_notifier = nullptr;
            }
            ::close(m_fd);
            m_fd = -1;
        }

        const QByteArray path = port.toUtf8();
        m_fd = ::open(path.constData(), O_RDWR | O_NOCTTY | O_SYNC);
        if (m_fd < 0) {
            const QString err = QString::fromUtf8(strerror(errno));
            qDebug() << "SerialBridge: open failed" << port << err;
            m_bridge->m_connected = false;
            m_bridge->m_portName.clear();
            emit m_bridge->connectionChanged();
            emit m_bridge->connectionLost(err);
            return;
        }
        if (configurePort(m_fd) < 0) {
            qWarning() << "SerialBridge: termios failed on" << port;
            ::close(m_fd);
            m_fd = -1;
            m_bridge->m_connected = false;
            m_bridge->m_portName.clear();
            emit m_bridge->connectionChanged();
            emit m_bridge->connectionLost(QStringLiteral("termios failed"));
            return;
        }

        qInfo() << "SerialBridge: connected to" << port;
        m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
        connect(m_notifier, &QSocketNotifier::activated, this, &SerialWorker::onReadyRead);

        m_portPath = port;
        m_bridge->m_connected = true;
        m_bridge->m_portName = port;
        m_badChecksum = 0;
        QSettings s;
        s.setValue(QStringLiteral("hal/serialPort"), port);
        if (m_linkWatch)
            m_linkWatch->start();
        emit m_bridge->connectionChanged();
    }

    void disconnectPort()
    {
        if (m_linkWatch)
            m_linkWatch->stop();
        if (m_notifier) {
            m_notifier->setEnabled(false);
            delete m_notifier;
            m_notifier = nullptr;
        }
        if (m_fd >= 0) {
            ::close(m_fd);
            m_fd = -1;
        }
        m_bridge->m_connected = false;
        m_bridge->m_portName.clear();
        emit m_bridge->connectionChanged();
    }

    void startVerify()
    {
        if (!beginOp())
            return;
        m_op = Op::Verify;
        m_deadlineMs = kVerifyTimeoutMs;
        writeAll(buildPacket(kCmdVerify));
        armDeadline();
    }

    void requestKey()
    {
        if (m_fd >= 0)
            writeAll(buildPacket(kCmdGetKey));
    }

    void startEnroll(int fingerId)
    {
        if (!beginOp())
            return;
        m_op = Op::Enroll;
        m_pendingEnrollFingerId = fingerId;
        m_deadlineMs = kEnrollTimeoutMs;
        QByteArray pl;
        pl.append(static_cast<char>(fingerId & 0xFF));
        writeAll(buildPacket(kCmdEnroll, pl));
        armDeadline();
    }

    void deleteFinger(int fingerId)
    {
        if (!beginOp())
            return;
        m_op = Op::Delete;
        m_deadlineMs = kVerifyTimeoutMs;
        m_pendingDeleteFingerId = fingerId;
        QByteArray pl;
        pl.append(static_cast<char>(fingerId & 0xFF));
        writeAll(buildPacket(kCmdDelete, pl));
        armDeadline();
    }

    void listEnrolled()
    {
        if (!beginOp())
            return;
        m_op = Op::List;
        m_deadlineMs = kVerifyTimeoutMs;
        writeAll(buildPacket(kCmdListEnrolled));
        armDeadline();
    }

    void cancelOperation()
    {
        const Op was = m_op;
        if (m_fd >= 0)
            writeAll(buildPacket(kCmdCancel));
        finishOp();
        if (was == Op::Verify)
            emit m_bridge->verifyCompleted(false, 0, QStringLiteral("Verification cancelled"));
        else if (was == Op::Enroll)
            emit m_bridge->enrollCompleted(false, m_pendingEnrollFingerId,
                                            QStringLiteral("Enroll cancelled"));
        else if (was == Op::Delete)
            emit m_bridge->deleteCompleted(false, m_pendingDeleteFingerId,
                                            QStringLiteral("Delete cancelled"));
        else if (was == Op::List)
            emit m_bridge->listEnrolledCompleted(false, {}, QStringLiteral("List cancelled"));
    }

private slots:
    void onReadyRead()
    {
        char buf[256];
        for (;;) {
            const ssize_t n = ::read(m_fd, buf, sizeof(buf));
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                reportLinkLost(QString::fromUtf8(strerror(errno)));
                return;
            }
            if (n == 0) {
                reportLinkLost(QStringLiteral("device_removed"));
                return;
            }
            m_rx.append(buf, static_cast<int>(n));
        }
        processRx();
    }

    void checkLink()
    {
        if (m_fd < 0 || !m_bridge->m_connected)
            return;
        if (!m_portPath.isEmpty() && !QFile::exists(m_portPath)) {
            reportLinkLost(QStringLiteral("device_removed"));
            return;
        }
        struct pollfd pfd;
        pfd.fd = m_fd;
        pfd.events = POLLIN | POLLERR | POLLHUP;
        pfd.revents = 0;
        const int pr = ::poll(&pfd, 1, 0);
        if (pr < 0) {
            if (errno != EINTR)
                reportLinkLost(QString::fromUtf8(strerror(errno)));
            return;
        }
        if (pr > 0 && (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))) {
            reportLinkLost(QStringLiteral("device_removed"));
        }
    }

    void reportLinkLost(const QString &reason)
    {
        if (m_fd < 0 && !m_bridge->m_connected)
            return;
        if (m_linkWatch)
            m_linkWatch->stop();
        if (m_timer)
            m_timer->stop();
        if (m_recoveryTimer)
            m_recoveryTimer->stop();
        m_op = Op::None;
        m_timeoutOp = Op::None;
        if (m_bridge->m_busy.load(std::memory_order_acquire))
            m_bridge->setBusy(false);
        if (m_notifier) {
            m_notifier->setEnabled(false);
            delete m_notifier;
            m_notifier = nullptr;
        }
        if (m_fd >= 0) {
            ::close(m_fd);
            m_fd = -1;
        }
        m_rx.clear();
        const bool wasUp = m_bridge->m_connected;
        m_bridge->m_connected = false;
        m_bridge->m_portName.clear();
        if (!wasUp)
            return;
        emit m_bridge->connectionChanged();
        emit m_bridge->connectionLost(reason);
    }

    void onDeadline()
    {
        if (m_op == Op::None || !m_timer || !m_recoveryTimer)
            return;
        m_timeoutOp = m_op;
        m_op = Op::None;
        m_timer->stop();
        if (m_fd >= 0)
            writeAll(buildPacket(kCmdCancel));
        m_recoveryTimer->start(kPostCancelMs);
    }

    void onRecoveryDone()
    {
        const Op timed = m_timeoutOp;
        m_timeoutOp = Op::None;
        resetPort();
        if (m_bridge->m_busy.load(std::memory_order_acquire))
            m_bridge->setBusy(false);
        if (timed == Op::Verify)
            emit m_bridge->verifyCompleted(false, 0, QStringLiteral("Verification timed out"));
        else if (timed == Op::Enroll)
            emit m_bridge->enrollCompleted(false, m_pendingEnrollFingerId,
                                            QStringLiteral("Enroll timed out"));
        else if (timed == Op::Delete)
            emit m_bridge->deleteCompleted(false, m_pendingDeleteFingerId,
                                            QStringLiteral("Delete timed out"));
        else if (timed == Op::List)
            emit m_bridge->listEnrolledCompleted(false, {}, QStringLiteral("List timed out"));
    }

    void resetPort()
    {
        if (m_notifier) {
            m_notifier->setEnabled(false);
            delete m_notifier;
            m_notifier = nullptr;
        }
        if (m_fd >= 0) {
            ::close(m_fd);
            m_fd = -1;
        }
        m_rx.clear();
        if (!m_portPath.isEmpty())
            connectPort(m_portPath);
    }

private:
    enum class Op { None, Verify, Enroll, Delete, List };

    void writeAll(const QByteArray &data)
    {
        if (m_fd < 0)
            return;
        const char *p = data.constData();
        ssize_t left = data.size();
        while (left > 0) {
            const ssize_t w = ::write(m_fd, p, static_cast<size_t>(left));
            if (w < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                reportLinkLost(QString::fromUtf8(strerror(errno)));
                return;
            }
            if (w == 0) {
                reportLinkLost(QStringLiteral("device_removed"));
                return;
            }
            p += w;
            left -= w;
        }
    }

    bool beginOp()
    {
        if (m_fd < 0) {
            emit m_bridge->connectionLost(QStringLiteral("port closed"));
            return false;
        }
        if (m_bridge->m_busy.load(std::memory_order_acquire))
            return false;
        m_bridge->setBusy(true);
        m_rx.clear();
        return true;
    }

    void finishOp()
    {
        if (m_timer)
            m_timer->stop();
        m_op = Op::None;
        if (m_bridge->m_busy.load(std::memory_order_acquire))
            m_bridge->setBusy(false);
    }

    void armDeadline()
    {
        if (m_timer)
            m_timer->start(m_deadlineMs);
    }

    void processRx()
    {
        while (m_rx.size() >= 4) {
            int start = m_rx.indexOf(static_cast<char>(kSync));
            if (start < 0) {
                m_rx.clear();
                return;
            }
            if (start > 0)
                m_rx.remove(0, start);
            if (m_rx.size() < 3)
                return;
            const quint8 code = static_cast<quint8>(m_rx.at(1));
            const quint8 plen = static_cast<quint8>(m_rx.at(2));
            const int total = 4 + plen;
            if (m_rx.size() < total)
                return;
            const QByteArray frame = m_rx.left(total);
            m_rx.remove(0, total);
            const QByteArray body = frame.left(frame.size() - 1);
            if (xorChecksum(body) != static_cast<quint8>(frame.at(frame.size() - 1))) {
                m_badChecksum++;
                if (m_badChecksum >= 3) {
                    m_badChecksum = 0;
                    emit m_bridge->connectionLost(QStringLiteral("protocol_error"));
                    resetPort();
                }
                continue;
            }
            m_badChecksum = 0;
            handlePacket(code, frame.mid(3, plen));
        }
    }

    void handlePacket(quint8 code, const QByteArray &payload)
    {
        if (code >= kEvtMin && code <= kEvtMax) {
            emit m_bridge->sensorEvent(static_cast<int>(code));
            return;
        }
        if (code == kResKey && payload.size() == 32) {
            emit m_bridge->keyReceived(payload);
            finishOp();
            return;
        }
        if (m_op == Op::Delete) {
            const int fid = m_pendingDeleteFingerId;
            if (code == kResSuccess) {
                finishOp();
                emit m_bridge->deleteCompleted(true, fid, {});
            } else if (code >= kResFailNoFinger) {
                finishOp();
                const QString err = (code == kResNoMatch)
                    ? QStringLiteral("Fingerprint slot #%1 not found on sensor").arg(fid)
                    : enrollErrorMessage(code, payload);
                emit m_bridge->deleteCompleted(false, fid, err);
            }
            return;
        }
        if (m_op == Op::Verify) {
            if (code == kResSuccess && !payload.isEmpty()) {
                const int mid = static_cast<quint8>(payload.at(0));
                finishOp();
                emit m_bridge->verifyCompleted(true, mid, {});
                return;
            }
            if (code >= kResFailNoFinger) {
                finishOp();
                emit m_bridge->verifyCompleted(false, 0, verifyErrorMessage(code));
            }
            return;
        }
        if (m_op == Op::Enroll) {
            const int attempted = m_pendingEnrollFingerId;
            if (code == kResSuccess && !payload.isEmpty()) {
                const int fid = static_cast<quint8>(payload.at(0));
                finishOp();
                emit m_bridge->enrollCompleted(true, fid, {});
            } else if (code >= kResFailNoFinger) {
                finishOp();
                int fid = attempted;
                if (!payload.isEmpty())
                    fid = static_cast<quint8>(payload.at(0));
                emit m_bridge->enrollCompleted(false, fid, enrollErrorMessage(code, payload));
            }
            return;
        }
        if (m_op == Op::List) {
            if (code == kResSuccess && !payload.isEmpty()) {
                const int count = static_cast<quint8>(payload.at(0));
                QList<int> ids;
                for (int i = 0; i < count && (1 + i) < payload.size(); ++i)
                    ids.append(static_cast<quint8>(payload.at(1 + i)));
                finishOp();
                emit m_bridge->listEnrolledCompleted(true, ids, {});
            } else if (code >= kResFailNoFinger) {
                finishOp();
                emit m_bridge->listEnrolledCompleted(
                    false, {}, enrollErrorMessage(code, payload));
            }
            return;
        }
    }

    SerialBridge *m_bridge;
    QString m_portPath;
    int m_fd = -1;
    QSocketNotifier *m_notifier = nullptr;
    QByteArray m_rx;
    QTimer *m_timer = nullptr;
    QTimer *m_recoveryTimer = nullptr;
    QTimer *m_linkWatch = nullptr;
    Op m_op = Op::None;
    Op m_timeoutOp = Op::None;
    int m_deadlineMs = kVerifyTimeoutMs;
    int m_badChecksum = 0;
    int m_pendingDeleteFingerId = -1;
    int m_pendingEnrollFingerId = -1;
};

SerialBridge::SerialBridge(QObject *parent)
    : QObject(parent)
    , m_thread(new QThread(this))
    , m_worker(new SerialWorker(this))
{
    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::started, m_worker, &SerialWorker::setupOnWorkerThread);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_thread->start();
}

SerialBridge::~SerialBridge()
{
    if (m_worker && m_thread->isRunning()) {
        QMetaObject::invokeMethod(m_worker, "disconnectPort", Qt::BlockingQueuedConnection);
        m_thread->quit();
        m_thread->wait(2000);
    }
}

void SerialBridge::connectToPort(const QString &port)
{
    QMetaObject::invokeMethod(m_worker, "connectPort", Qt::QueuedConnection,
                              Q_ARG(QString, port));
}

void SerialBridge::disconnectPort()
{
    QMetaObject::invokeMethod(m_worker, "disconnectPort", Qt::QueuedConnection);
}

void SerialBridge::startVerify()
{
    QMetaObject::invokeMethod(m_worker, "startVerify", Qt::QueuedConnection);
}

void SerialBridge::requestKey()
{
    QMetaObject::invokeMethod(m_worker, "requestKey", Qt::QueuedConnection);
}

void SerialBridge::startEnroll(int fingerId)
{
    QMetaObject::invokeMethod(m_worker, "startEnroll", Qt::QueuedConnection,
                              Q_ARG(int, fingerId));
}

void SerialBridge::deleteFinger(int fingerId)
{
    QMetaObject::invokeMethod(m_worker, "deleteFinger", Qt::QueuedConnection,
                              Q_ARG(int, fingerId));
}

void SerialBridge::cancelOperation()
{
    QMetaObject::invokeMethod(m_worker, "cancelOperation", Qt::QueuedConnection);
}

void SerialBridge::listEnrolled()
{
    QMetaObject::invokeMethod(m_worker, "listEnrolled", Qt::QueuedConnection);
}

#include "serial_bridge.moc"
