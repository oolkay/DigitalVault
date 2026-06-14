/****************************************************************************
** Meta object code from reading C++ file 'serial_bridge.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.2.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../gui/serial_bridge.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#include <QtCore/QList>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'serial_bridge.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.2.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_SerialBridge_t {
    const uint offsetsAndSize[68];
    char stringdata0[408];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(offsetof(qt_meta_stringdata_SerialBridge_t, stringdata0) + ofs), len 
static const qt_meta_stringdata_SerialBridge_t qt_meta_stringdata_SerialBridge = {
    {
QT_MOC_LITERAL(0, 12), // "SerialBridge"
QT_MOC_LITERAL(13, 17), // "connectionChanged"
QT_MOC_LITERAL(31, 0), // ""
QT_MOC_LITERAL(32, 11), // "busyChanged"
QT_MOC_LITERAL(44, 17), // "operationRejected"
QT_MOC_LITERAL(62, 6), // "reason"
QT_MOC_LITERAL(69, 11), // "sensorEvent"
QT_MOC_LITERAL(81, 9), // "eventCode"
QT_MOC_LITERAL(91, 15), // "verifyCompleted"
QT_MOC_LITERAL(107, 7), // "success"
QT_MOC_LITERAL(115, 9), // "matchedId"
QT_MOC_LITERAL(125, 5), // "error"
QT_MOC_LITERAL(131, 11), // "keyReceived"
QT_MOC_LITERAL(143, 5), // "key32"
QT_MOC_LITERAL(149, 15), // "enrollCompleted"
QT_MOC_LITERAL(165, 8), // "fingerId"
QT_MOC_LITERAL(174, 15), // "deleteCompleted"
QT_MOC_LITERAL(190, 21), // "listEnrolledCompleted"
QT_MOC_LITERAL(212, 10), // "QList<int>"
QT_MOC_LITERAL(223, 9), // "fingerIds"
QT_MOC_LITERAL(233, 14), // "connectionLost"
QT_MOC_LITERAL(248, 13), // "connectToPort"
QT_MOC_LITERAL(262, 4), // "port"
QT_MOC_LITERAL(267, 14), // "disconnectPort"
QT_MOC_LITERAL(282, 11), // "startVerify"
QT_MOC_LITERAL(294, 10), // "requestKey"
QT_MOC_LITERAL(305, 11), // "startEnroll"
QT_MOC_LITERAL(317, 12), // "deleteFinger"
QT_MOC_LITERAL(330, 15), // "cancelOperation"
QT_MOC_LITERAL(346, 12), // "listEnrolled"
QT_MOC_LITERAL(359, 24), // "cancelInventoryIfRunning"
QT_MOC_LITERAL(384, 9), // "connected"
QT_MOC_LITERAL(394, 8), // "portName"
QT_MOC_LITERAL(403, 4) // "busy"

    },
    "SerialBridge\0connectionChanged\0\0"
    "busyChanged\0operationRejected\0reason\0"
    "sensorEvent\0eventCode\0verifyCompleted\0"
    "success\0matchedId\0error\0keyReceived\0"
    "key32\0enrollCompleted\0fingerId\0"
    "deleteCompleted\0listEnrolledCompleted\0"
    "QList<int>\0fingerIds\0connectionLost\0"
    "connectToPort\0port\0disconnectPort\0"
    "startVerify\0requestKey\0startEnroll\0"
    "deleteFinger\0cancelOperation\0listEnrolled\0"
    "cancelInventoryIfRunning\0connected\0"
    "portName\0busy"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_SerialBridge[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
      19,   14, // methods
       3,  185, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
      10,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,  128,    2, 0x06,    4 /* Public */,
       3,    0,  129,    2, 0x06,    5 /* Public */,
       4,    1,  130,    2, 0x06,    6 /* Public */,
       6,    1,  133,    2, 0x06,    8 /* Public */,
       8,    3,  136,    2, 0x06,   10 /* Public */,
      12,    1,  143,    2, 0x06,   14 /* Public */,
      14,    3,  146,    2, 0x06,   16 /* Public */,
      16,    3,  153,    2, 0x06,   20 /* Public */,
      17,    3,  160,    2, 0x06,   24 /* Public */,
      20,    1,  167,    2, 0x06,   28 /* Public */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
      21,    1,  170,    2, 0x02,   30 /* Public */,
      23,    0,  173,    2, 0x02,   32 /* Public */,
      24,    0,  174,    2, 0x02,   33 /* Public */,
      25,    0,  175,    2, 0x02,   34 /* Public */,
      26,    1,  176,    2, 0x02,   35 /* Public */,
      27,    1,  179,    2, 0x02,   37 /* Public */,
      28,    0,  182,    2, 0x02,   39 /* Public */,
      29,    0,  183,    2, 0x02,   40 /* Public */,
      30,    0,  184,    2, 0x02,   41 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    5,
    QMetaType::Void, QMetaType::Int,    7,
    QMetaType::Void, QMetaType::Bool, QMetaType::Int, QMetaType::QString,    9,   10,   11,
    QMetaType::Void, QMetaType::QByteArray,   13,
    QMetaType::Void, QMetaType::Bool, QMetaType::Int, QMetaType::QString,    9,   15,   11,
    QMetaType::Void, QMetaType::Bool, QMetaType::Int, QMetaType::QString,    9,   15,   11,
    QMetaType::Void, QMetaType::Bool, 0x80000000 | 18, QMetaType::QString,    9,   19,   11,
    QMetaType::Void, QMetaType::QString,    5,

 // methods: parameters
    QMetaType::Void, QMetaType::QString,   22,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   15,
    QMetaType::Void, QMetaType::Int,   15,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

 // properties: name, type, flags
      31, QMetaType::Bool, 0x00015001, uint(0), 0,
      32, QMetaType::QString, 0x00015001, uint(0), 0,
      33, QMetaType::Bool, 0x00015001, uint(1), 0,

       0        // eod
};

void SerialBridge::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<SerialBridge *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->connectionChanged(); break;
        case 1: _t->busyChanged(); break;
        case 2: _t->operationRejected((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 3: _t->sensorEvent((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 4: _t->verifyCompleted((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 5: _t->keyReceived((*reinterpret_cast< std::add_pointer_t<QByteArray>>(_a[1]))); break;
        case 6: _t->enrollCompleted((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 7: _t->deleteCompleted((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 8: _t->listEnrolledCompleted((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QList<int>>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 9: _t->connectionLost((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 10: _t->connectToPort((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 11: _t->disconnectPort(); break;
        case 12: _t->startVerify(); break;
        case 13: _t->requestKey(); break;
        case 14: _t->startEnroll((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 15: _t->deleteFinger((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 16: _t->cancelOperation(); break;
        case 17: _t->listEnrolled(); break;
        case 18: _t->cancelInventoryIfRunning(); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 8:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QList<int> >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (SerialBridge::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&SerialBridge::connectionChanged)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (SerialBridge::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&SerialBridge::busyChanged)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (SerialBridge::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&SerialBridge::operationRejected)) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (SerialBridge::*)(int );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&SerialBridge::sensorEvent)) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (SerialBridge::*)(bool , int , const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&SerialBridge::verifyCompleted)) {
                *result = 4;
                return;
            }
        }
        {
            using _t = void (SerialBridge::*)(const QByteArray & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&SerialBridge::keyReceived)) {
                *result = 5;
                return;
            }
        }
        {
            using _t = void (SerialBridge::*)(bool , int , const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&SerialBridge::enrollCompleted)) {
                *result = 6;
                return;
            }
        }
        {
            using _t = void (SerialBridge::*)(bool , int , const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&SerialBridge::deleteCompleted)) {
                *result = 7;
                return;
            }
        }
        {
            using _t = void (SerialBridge::*)(bool , const QList<int> & , const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&SerialBridge::listEnrolledCompleted)) {
                *result = 8;
                return;
            }
        }
        {
            using _t = void (SerialBridge::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&SerialBridge::connectionLost)) {
                *result = 9;
                return;
            }
        }
    }
#ifndef QT_NO_PROPERTIES
    else if (_c == QMetaObject::ReadProperty) {
        auto *_t = static_cast<SerialBridge *>(_o);
        (void)_t;
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< bool*>(_v) = _t->isConnected(); break;
        case 1: *reinterpret_cast< QString*>(_v) = _t->portName(); break;
        case 2: *reinterpret_cast< bool*>(_v) = _t->isBusy(); break;
        default: break;
        }
    } else if (_c == QMetaObject::WriteProperty) {
    } else if (_c == QMetaObject::ResetProperty) {
    } else if (_c == QMetaObject::BindableProperty) {
    }
#endif // QT_NO_PROPERTIES
}

const QMetaObject SerialBridge::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_SerialBridge.offsetsAndSize,
    qt_meta_data_SerialBridge,
    qt_static_metacall,
    nullptr,
qt_incomplete_metaTypeArray<qt_meta_stringdata_SerialBridge_t
, QtPrivate::TypeAndForceComplete<bool, std::true_type>, QtPrivate::TypeAndForceComplete<QString, std::true_type>, QtPrivate::TypeAndForceComplete<bool, std::true_type>, QtPrivate::TypeAndForceComplete<SerialBridge, std::true_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<const QString &, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<bool, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<const QString &, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<const QByteArray &, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<bool, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<const QString &, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<bool, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<const QString &, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<bool, std::false_type>, QtPrivate::TypeAndForceComplete<const QList<int> &, std::false_type>, QtPrivate::TypeAndForceComplete<const QString &, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<const QString &, std::false_type>

, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<const QString &, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>

>,
    nullptr
} };


const QMetaObject *SerialBridge::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SerialBridge::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_SerialBridge.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int SerialBridge::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 19)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 19;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 19)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 19;
    }
#ifndef QT_NO_PROPERTIES
    else if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    }
#endif // QT_NO_PROPERTIES
    return _id;
}

// SIGNAL 0
void SerialBridge::connectionChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void SerialBridge::busyChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void SerialBridge::operationRejected(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void SerialBridge::sensorEvent(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void SerialBridge::verifyCompleted(bool _t1, int _t2, const QString & _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void SerialBridge::keyReceived(const QByteArray & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}

// SIGNAL 6
void SerialBridge::enrollCompleted(bool _t1, int _t2, const QString & _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 6, _a);
}

// SIGNAL 7
void SerialBridge::deleteCompleted(bool _t1, int _t2, const QString & _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 7, _a);
}

// SIGNAL 8
void SerialBridge::listEnrolledCompleted(bool _t1, const QList<int> & _t2, const QString & _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 8, _a);
}

// SIGNAL 9
void SerialBridge::connectionLost(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 9, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
