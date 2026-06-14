/****************************************************************************
** Meta object code from reading C++ file 'vault_controller.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.2.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../gui/vault_controller.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'vault_controller.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.2.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_VaultController_t {
    const uint offsetsAndSize[352];
    char stringdata0[3056];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(offsetof(qt_meta_stringdata_VaultController_t, stringdata0) + ofs), len 
static const qt_meta_stringdata_VaultController_t qt_meta_stringdata_VaultController = {
    {
QT_MOC_LITERAL(0, 15), // "VaultController"
QT_MOC_LITERAL(16, 11), // "QML.Element"
QT_MOC_LITERAL(28, 4), // "auto"
QT_MOC_LITERAL(33, 13), // "statusChanged"
QT_MOC_LITERAL(47, 0), // ""
QT_MOC_LITERAL(48, 19), // "fingerprintsChanged"
QT_MOC_LITERAL(68, 11), // "logsChanged"
QT_MOC_LITERAL(80, 20), // "notificationReceived"
QT_MOC_LITERAL(101, 7), // "message"
QT_MOC_LITERAL(109, 7), // "isError"
QT_MOC_LITERAL(117, 19), // "setupRunningChanged"
QT_MOC_LITERAL(137, 13), // "setupFinished"
QT_MOC_LITERAL(151, 7), // "success"
QT_MOC_LITERAL(159, 18), // "loopCreateFinished"
QT_MOC_LITERAL(178, 20), // "deviceDeleteFinished"
QT_MOC_LITERAL(199, 19), // "storagePlanFinished"
QT_MOC_LITERAL(219, 14), // "devicesChanged"
QT_MOC_LITERAL(234, 15), // "togglingChanged"
QT_MOC_LITERAL(250, 30), // "fingerprintUnlockActiveChanged"
QT_MOC_LITERAL(281, 29), // "fingerprintPromptStateChanged"
QT_MOC_LITERAL(311, 28), // "fingerprintEnrollStepChanged"
QT_MOC_LITERAL(340, 25), // "setupOwnerFingerIdChanged"
QT_MOC_LITERAL(366, 22), // "setupOwnerLabelChanged"
QT_MOC_LITERAL(389, 25), // "setupAccessEntriesChanged"
QT_MOC_LITERAL(415, 21), // "deleteVaultGatePassed"
QT_MOC_LITERAL(437, 10), // "devicePath"
QT_MOC_LITERAL(448, 22), // "vaultAccessListChanged"
QT_MOC_LITERAL(471, 22), // "autoLockTimeoutChanged"
QT_MOC_LITERAL(494, 19), // "navigationRequested"
QT_MOC_LITERAL(514, 8), // "pageFile"
QT_MOC_LITERAL(523, 19), // "sensorStatusChanged"
QT_MOC_LITERAL(543, 20), // "autoDetectUsbChanged"
QT_MOC_LITERAL(564, 33), // "hardwareDisconnectLockdownCha..."
QT_MOC_LITERAL(598, 29), // "lockRequiresForceConfirmation"
QT_MOC_LITERAL(628, 13), // "onSimFinished"
QT_MOC_LITERAL(642, 8), // "exitCode"
QT_MOC_LITERAL(651, 20), // "QProcess::ExitStatus"
QT_MOC_LITERAL(672, 10), // "exitStatus"
QT_MOC_LITERAL(683, 25), // "onSerialConnectionChanged"
QT_MOC_LITERAL(709, 10), // "pollStatus"
QT_MOC_LITERAL(720, 11), // "toggleVault"
QT_MOC_LITERAL(732, 14), // "forceLockVault"
QT_MOC_LITERAL(747, 22), // "beginFingerprintUnlock"
QT_MOC_LITERAL(770, 23), // "cancelFingerprintUnlock"
QT_MOC_LITERAL(794, 21), // "continueAfterDecision"
QT_MOC_LITERAL(816, 21), // "beginSetupOwnerEnroll"
QT_MOC_LITERAL(838, 8), // "fingerId"
QT_MOC_LITERAL(847, 30), // "beginSetupOwnerEnrollForDevice"
QT_MOC_LITERAL(878, 21), // "prepareSetupOwnerSlot"
QT_MOC_LITERAL(900, 23), // "clearSetupWizardSession"
QT_MOC_LITERAL(924, 24), // "ownerFingerSlotForDevice"
QT_MOC_LITERAL(949, 18), // "nextFreeFingerSlot"
QT_MOC_LITERAL(968, 17), // "excludeDevicePath"
QT_MOC_LITERAL(986, 24), // "beginVaultSetupBiometric"
QT_MOC_LITERAL(1011, 6), // "device"
QT_MOC_LITERAL(1018, 5), // "force"
QT_MOC_LITERAL(1024, 11), // "vaultSizeMB"
QT_MOC_LITERAL(1036, 9), // "vaultName"
QT_MOC_LITERAL(1046, 10), // "normalName"
QT_MOC_LITERAL(1057, 8), // "normalFs"
QT_MOC_LITERAL(1066, 18), // "fingerInAccessList"
QT_MOC_LITERAL(1085, 19), // "accessListForDevice"
QT_MOC_LITERAL(1105, 14), // "addFingerprint"
QT_MOC_LITERAL(1120, 17), // "removeFingerprint"
QT_MOC_LITERAL(1138, 5), // "index"
QT_MOC_LITERAL(1144, 17), // "renameFingerprint"
QT_MOC_LITERAL(1162, 7), // "newName"
QT_MOC_LITERAL(1170, 17), // "getStorageDevices"
QT_MOC_LITERAL(1188, 16), // "fingerprintLabel"
QT_MOC_LITERAL(1205, 19), // "refreshFingerprints"
QT_MOC_LITERAL(1225, 14), // "startSimulator"
QT_MOC_LITERAL(1240, 13), // "stopSimulator"
QT_MOC_LITERAL(1254, 15), // "reconnectSerial"
QT_MOC_LITERAL(1270, 23), // "connectToConfiguredPort"
QT_MOC_LITERAL(1294, 4), // "port"
QT_MOC_LITERAL(1299, 20), // "availableSerialPorts"
QT_MOC_LITERAL(1320, 23), // "clearSerialPortSettings"
QT_MOC_LITERAL(1344, 17), // "setSelectedDevice"
QT_MOC_LITERAL(1362, 4), // "path"
QT_MOC_LITERAL(1367, 18), // "createSetupSession"
QT_MOC_LITERAL(1386, 20), // "defaultContainersDir"
QT_MOC_LITERAL(1407, 16), // "createLoopDevice"
QT_MOC_LITERAL(1424, 13), // "containerName"
QT_MOC_LITERAL(1438, 11), // "totalSizeMB"
QT_MOC_LITERAL(1450, 10), // "storageDir"
QT_MOC_LITERAL(1461, 12), // "deleteDevice"
QT_MOC_LITERAL(1474, 11), // "deleteVault"
QT_MOC_LITERAL(1486, 28), // "deleteVaultRequiresBiometric"
QT_MOC_LITERAL(1515, 18), // "previewDeleteVault"
QT_MOC_LITERAL(1534, 22), // "renameVaultDisplayName"
QT_MOC_LITERAL(1557, 21), // "reconcileVaultRuntime"
QT_MOC_LITERAL(1579, 22), // "storageLayoutForDevice"
QT_MOC_LITERAL(1602, 8), // "diskPath"
QT_MOC_LITERAL(1611, 25), // "availablePartitionActions"
QT_MOC_LITERAL(1637, 8), // "partPath"
QT_MOC_LITERAL(1646, 16), // "buildStoragePlan"
QT_MOC_LITERAL(1663, 8), // "actionId"
QT_MOC_LITERAL(1672, 6), // "params"
QT_MOC_LITERAL(1679, 18), // "previewStoragePlan"
QT_MOC_LITERAL(1698, 4), // "plan"
QT_MOC_LITERAL(1703, 16), // "applyStoragePlan"
QT_MOC_LITERAL(1720, 20), // "supportedFilesystems"
QT_MOC_LITERAL(1741, 22), // "resolveManagedDiskPath"
QT_MOC_LITERAL(1764, 12), // "registryPath"
QT_MOC_LITERAL(1777, 24), // "registryBackingForDevice"
QT_MOC_LITERAL(1802, 14), // "emergencyClose"
QT_MOC_LITERAL(1817, 16), // "vaultRuntimeOpen"
QT_MOC_LITERAL(1834, 10), // "setupVault"
QT_MOC_LITERAL(1845, 7), // "ownerId"
QT_MOC_LITERAL(1853, 17), // "openInFileManager"
QT_MOC_LITERAL(1871, 14), // "openInTerminal"
QT_MOC_LITERAL(1886, 28), // "navigateToVaultManagerDetail"
QT_MOC_LITERAL(1915, 33), // "takePendingVaultManagerDetail..."
QT_MOC_LITERAL(1949, 15), // "savedSerialPort"
QT_MOC_LITERAL(1965, 11), // "verifyOwner"
QT_MOC_LITERAL(1977, 11), // "grantAccess"
QT_MOC_LITERAL(1989, 13), // "ownerFingerId"
QT_MOC_LITERAL(2003, 11), // "newFingerId"
QT_MOC_LITERAL(2015, 22), // "beginGrantAccessEnroll"
QT_MOC_LITERAL(2038, 5), // "label"
QT_MOC_LITERAL(2044, 24), // "beginGrantAccessForVault"
QT_MOC_LITERAL(2069, 25), // "beginRevokeAccessForVault"
QT_MOC_LITERAL(2095, 22), // "beginSetupAccessEnroll"
QT_MOC_LITERAL(2118, 24), // "removePendingSetupAccess"
QT_MOC_LITERAL(2143, 23), // "clearPendingSetupAccess"
QT_MOC_LITERAL(2167, 29), // "beginDeleteVaultBiometricGate"
QT_MOC_LITERAL(2197, 12), // "revokeAccess"
QT_MOC_LITERAL(2210, 10), // "slotNumber"
QT_MOC_LITERAL(2221, 23), // "isFingerAllowedForVault"
QT_MOC_LITERAL(2245, 19), // "labelExistsForVault"
QT_MOC_LITERAL(2265, 27), // "renameVaultFingerprintLabel"
QT_MOC_LITERAL(2293, 8), // "newLabel"
QT_MOC_LITERAL(2302, 11), // "getKeyslots"
QT_MOC_LITERAL(2314, 20), // "getKeyslotsForDevice"
QT_MOC_LITERAL(2335, 20), // "vaultDetailForDevice"
QT_MOC_LITERAL(2356, 24), // "isVaultUnlockedForDevice"
QT_MOC_LITERAL(2381, 19), // "getSecurityLogsPage"
QT_MOC_LITERAL(2401, 4), // "page"
QT_MOC_LITERAL(2406, 8), // "pageSize"
QT_MOC_LITERAL(2415, 9), // "clearLogs"
QT_MOC_LITERAL(2425, 10), // "notifyUser"
QT_MOC_LITERAL(2436, 14), // "navigateToPage"
QT_MOC_LITERAL(2451, 8), // "isLocked"
QT_MOC_LITERAL(2460, 15), // "vaultStatusText"
QT_MOC_LITERAL(2476, 16), // "fingerprintCount"
QT_MOC_LITERAL(2493, 16), // "simulatorRunning"
QT_MOC_LITERAL(2510, 14), // "selectedDevice"
QT_MOC_LITERAL(2525, 14), // "unlockedDevice"
QT_MOC_LITERAL(2540, 12), // "securityLogs"
QT_MOC_LITERAL(2553, 14), // "isSetupRunning"
QT_MOC_LITERAL(2568, 10), // "isToggling"
QT_MOC_LITERAL(2579, 23), // "fingerprintUnlockActive"
QT_MOC_LITERAL(2603, 24), // "fingerprintUnlockMessage"
QT_MOC_LITERAL(2628, 15), // "fingerprintFlow"
QT_MOC_LITERAL(2644, 22), // "fingerprintPromptState"
QT_MOC_LITERAL(2667, 27), // "fingerprintAwaitingDecision"
QT_MOC_LITERAL(2695, 21), // "fingerprintEnrollStep"
QT_MOC_LITERAL(2717, 18), // "setupOwnerFingerId"
QT_MOC_LITERAL(2736, 15), // "setupOwnerLabel"
QT_MOC_LITERAL(2752, 18), // "setupAccessEntries"
QT_MOC_LITERAL(2771, 12), // "serialBridge"
QT_MOC_LITERAL(2784, 15), // "sensorConnected"
QT_MOC_LITERAL(2800, 12), // "sensorStatus"
QT_MOC_LITERAL(2813, 16), // "sensorStatusText"
QT_MOC_LITERAL(2830, 14), // "sensorPortPath"
QT_MOC_LITERAL(2845, 13), // "autoDetectUsb"
QT_MOC_LITERAL(2859, 26), // "hardwareDisconnectLockdown"
QT_MOC_LITERAL(2886, 15), // "autoLockTimeout"
QT_MOC_LITERAL(2902, 15), // "setupStatusText"
QT_MOC_LITERAL(2918, 21), // "isSelectedDeviceVault"
QT_MOC_LITERAL(2940, 24), // "selectedDeviceCapacityMB"
QT_MOC_LITERAL(2965, 18), // "selectedDeviceInfo"
QT_MOC_LITERAL(2984, 14), // "storageDevices"
QT_MOC_LITERAL(2999, 16), // "registeredVaults"
QT_MOC_LITERAL(3016, 14), // "managedDevices"
QT_MOC_LITERAL(3031, 24) // "discoveredStorageDevices"

    },
    "VaultController\0QML.Element\0auto\0"
    "statusChanged\0\0fingerprintsChanged\0"
    "logsChanged\0notificationReceived\0"
    "message\0isError\0setupRunningChanged\0"
    "setupFinished\0success\0loopCreateFinished\0"
    "deviceDeleteFinished\0storagePlanFinished\0"
    "devicesChanged\0togglingChanged\0"
    "fingerprintUnlockActiveChanged\0"
    "fingerprintPromptStateChanged\0"
    "fingerprintEnrollStepChanged\0"
    "setupOwnerFingerIdChanged\0"
    "setupOwnerLabelChanged\0setupAccessEntriesChanged\0"
    "deleteVaultGatePassed\0devicePath\0"
    "vaultAccessListChanged\0autoLockTimeoutChanged\0"
    "navigationRequested\0pageFile\0"
    "sensorStatusChanged\0autoDetectUsbChanged\0"
    "hardwareDisconnectLockdownChanged\0"
    "lockRequiresForceConfirmation\0"
    "onSimFinished\0exitCode\0QProcess::ExitStatus\0"
    "exitStatus\0onSerialConnectionChanged\0"
    "pollStatus\0toggleVault\0forceLockVault\0"
    "beginFingerprintUnlock\0cancelFingerprintUnlock\0"
    "continueAfterDecision\0beginSetupOwnerEnroll\0"
    "fingerId\0beginSetupOwnerEnrollForDevice\0"
    "prepareSetupOwnerSlot\0clearSetupWizardSession\0"
    "ownerFingerSlotForDevice\0nextFreeFingerSlot\0"
    "excludeDevicePath\0beginVaultSetupBiometric\0"
    "device\0force\0vaultSizeMB\0vaultName\0"
    "normalName\0normalFs\0fingerInAccessList\0"
    "accessListForDevice\0addFingerprint\0"
    "removeFingerprint\0index\0renameFingerprint\0"
    "newName\0getStorageDevices\0fingerprintLabel\0"
    "refreshFingerprints\0startSimulator\0"
    "stopSimulator\0reconnectSerial\0"
    "connectToConfiguredPort\0port\0"
    "availableSerialPorts\0clearSerialPortSettings\0"
    "setSelectedDevice\0path\0createSetupSession\0"
    "defaultContainersDir\0createLoopDevice\0"
    "containerName\0totalSizeMB\0storageDir\0"
    "deleteDevice\0deleteVault\0"
    "deleteVaultRequiresBiometric\0"
    "previewDeleteVault\0renameVaultDisplayName\0"
    "reconcileVaultRuntime\0storageLayoutForDevice\0"
    "diskPath\0availablePartitionActions\0"
    "partPath\0buildStoragePlan\0actionId\0"
    "params\0previewStoragePlan\0plan\0"
    "applyStoragePlan\0supportedFilesystems\0"
    "resolveManagedDiskPath\0registryPath\0"
    "registryBackingForDevice\0emergencyClose\0"
    "vaultRuntimeOpen\0setupVault\0ownerId\0"
    "openInFileManager\0openInTerminal\0"
    "navigateToVaultManagerDetail\0"
    "takePendingVaultManagerDetailPath\0"
    "savedSerialPort\0verifyOwner\0grantAccess\0"
    "ownerFingerId\0newFingerId\0"
    "beginGrantAccessEnroll\0label\0"
    "beginGrantAccessForVault\0"
    "beginRevokeAccessForVault\0"
    "beginSetupAccessEnroll\0removePendingSetupAccess\0"
    "clearPendingSetupAccess\0"
    "beginDeleteVaultBiometricGate\0"
    "revokeAccess\0slotNumber\0isFingerAllowedForVault\0"
    "labelExistsForVault\0renameVaultFingerprintLabel\0"
    "newLabel\0getKeyslots\0getKeyslotsForDevice\0"
    "vaultDetailForDevice\0isVaultUnlockedForDevice\0"
    "getSecurityLogsPage\0page\0pageSize\0"
    "clearLogs\0notifyUser\0navigateToPage\0"
    "isLocked\0vaultStatusText\0fingerprintCount\0"
    "simulatorRunning\0selectedDevice\0"
    "unlockedDevice\0securityLogs\0isSetupRunning\0"
    "isToggling\0fingerprintUnlockActive\0"
    "fingerprintUnlockMessage\0fingerprintFlow\0"
    "fingerprintPromptState\0"
    "fingerprintAwaitingDecision\0"
    "fingerprintEnrollStep\0setupOwnerFingerId\0"
    "setupOwnerLabel\0setupAccessEntries\0"
    "serialBridge\0sensorConnected\0sensorStatus\0"
    "sensorStatusText\0sensorPortPath\0"
    "autoDetectUsb\0hardwareDisconnectLockdown\0"
    "autoLockTimeout\0setupStatusText\0"
    "isSelectedDeviceVault\0selectedDeviceCapacityMB\0"
    "selectedDeviceInfo\0storageDevices\0"
    "registeredVaults\0managedDevices\0"
    "discoveredStorageDevices"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_VaultController[] = {

 // content:
      10,       // revision
       0,       // classname
       1,   14, // classinfo
     106,   16, // methods
      34,  948, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
      25,       // signalCount

 // classinfo: key, value
       1,    2,

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       3,    0,  652,    4, 0x06,   35 /* Public */,
       5,    0,  653,    4, 0x06,   36 /* Public */,
       6,    0,  654,    4, 0x06,   37 /* Public */,
       7,    2,  655,    4, 0x06,   38 /* Public */,
      10,    0,  660,    4, 0x06,   41 /* Public */,
      11,    2,  661,    4, 0x06,   42 /* Public */,
      13,    2,  666,    4, 0x06,   45 /* Public */,
      14,    2,  671,    4, 0x06,   48 /* Public */,
      15,    2,  676,    4, 0x06,   51 /* Public */,
      16,    0,  681,    4, 0x06,   54 /* Public */,
      17,    0,  682,    4, 0x06,   55 /* Public */,
      18,    0,  683,    4, 0x06,   56 /* Public */,
      19,    0,  684,    4, 0x06,   57 /* Public */,
      20,    0,  685,    4, 0x06,   58 /* Public */,
      21,    0,  686,    4, 0x06,   59 /* Public */,
      22,    0,  687,    4, 0x06,   60 /* Public */,
      23,    0,  688,    4, 0x06,   61 /* Public */,
      24,    1,  689,    4, 0x06,   62 /* Public */,
      26,    0,  692,    4, 0x06,   64 /* Public */,
      27,    0,  693,    4, 0x06,   65 /* Public */,
      28,    1,  694,    4, 0x06,   66 /* Public */,
      30,    0,  697,    4, 0x06,   68 /* Public */,
      31,    0,  698,    4, 0x06,   69 /* Public */,
      32,    0,  699,    4, 0x06,   70 /* Public */,
      33,    1,  700,    4, 0x06,   71 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      34,    2,  703,    4, 0x08,   73 /* Private */,
      38,    0,  708,    4, 0x08,   76 /* Private */,
      39,    0,  709,    4, 0x08,   77 /* Private */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
      40,    0,  710,    4, 0x02,   78 /* Public */,
      41,    0,  711,    4, 0x02,   79 /* Public */,
      42,    0,  712,    4, 0x02,   80 /* Public */,
      43,    0,  713,    4, 0x02,   81 /* Public */,
      44,    0,  714,    4, 0x02,   82 /* Public */,
      45,    1,  715,    4, 0x02,   83 /* Public */,
      47,    1,  718,    4, 0x02,   85 /* Public */,
      48,    1,  721,    4, 0x02,   87 /* Public */,
      49,    0,  724,    4, 0x02,   89 /* Public */,
      50,    1,  725,    4, 0x102,   90 /* Public | MethodIsConst  */,
      51,    1,  728,    4, 0x102,   92 /* Public | MethodIsConst  */,
      51,    0,  731,    4, 0x122,   94 /* Public | MethodCloned | MethodIsConst  */,
      53,    6,  732,    4, 0x02,   95 /* Public */,
      60,    1,  745,    4, 0x102,  102 /* Public | MethodIsConst  */,
      61,    1,  748,    4, 0x102,  104 /* Public | MethodIsConst  */,
      62,    0,  751,    4, 0x02,  106 /* Public */,
      63,    1,  752,    4, 0x02,  107 /* Public */,
      65,    2,  755,    4, 0x02,  109 /* Public */,
      67,    0,  760,    4, 0x02,  112 /* Public */,
      68,    1,  761,    4, 0x102,  113 /* Public | MethodIsConst  */,
      69,    0,  764,    4, 0x02,  115 /* Public */,
      70,    0,  765,    4, 0x02,  116 /* Public */,
      71,    0,  766,    4, 0x02,  117 /* Public */,
      72,    0,  767,    4, 0x02,  118 /* Public */,
      73,    1,  768,    4, 0x02,  119 /* Public */,
      75,    0,  771,    4, 0x02,  121 /* Public */,
      76,    0,  772,    4, 0x02,  122 /* Public */,
      77,    1,  773,    4, 0x02,  123 /* Public */,
      79,    1,  776,    4, 0x102,  125 /* Public | MethodIsConst  */,
      79,    0,  779,    4, 0x122,  127 /* Public | MethodCloned | MethodIsConst  */,
      80,    0,  780,    4, 0x102,  128 /* Public | MethodIsConst  */,
      81,    3,  781,    4, 0x02,  129 /* Public */,
      81,    2,  788,    4, 0x22,  133 /* Public | MethodCloned */,
      85,    1,  793,    4, 0x02,  136 /* Public */,
      86,    1,  796,    4, 0x02,  138 /* Public */,
      87,    1,  799,    4, 0x102,  140 /* Public | MethodIsConst  */,
      88,    1,  802,    4, 0x102,  142 /* Public | MethodIsConst  */,
      89,    2,  805,    4, 0x02,  144 /* Public */,
      90,    0,  810,    4, 0x02,  147 /* Public */,
      91,    1,  811,    4, 0x102,  148 /* Public | MethodIsConst  */,
      93,    2,  814,    4, 0x102,  150 /* Public | MethodIsConst  */,
      93,    1,  819,    4, 0x122,  153 /* Public | MethodCloned | MethodIsConst  */,
      95,    2,  822,    4, 0x102,  155 /* Public | MethodIsConst  */,
      98,    1,  827,    4, 0x102,  158 /* Public | MethodIsConst  */,
     100,    1,  830,    4, 0x02,  160 /* Public */,
     101,    0,  833,    4, 0x102,  162 /* Public | MethodIsConst  */,
     102,    1,  834,    4, 0x102,  163 /* Public | MethodIsConst  */,
     104,    1,  837,    4, 0x102,  165 /* Public | MethodIsConst  */,
     105,    0,  840,    4, 0x02,  167 /* Public */,
     106,    0,  841,    4, 0x102,  168 /* Public | MethodIsConst  */,
     107,    7,  842,    4, 0x02,  169 /* Public */,
     109,    0,  857,    4, 0x02,  177 /* Public */,
     110,    0,  858,    4, 0x02,  178 /* Public */,
     111,    1,  859,    4, 0x02,  179 /* Public */,
     112,    0,  862,    4, 0x02,  181 /* Public */,
     113,    0,  863,    4, 0x102,  182 /* Public | MethodIsConst  */,
     114,    1,  864,    4, 0x02,  183 /* Public */,
     115,    2,  867,    4, 0x02,  185 /* Public */,
     118,    3,  872,    4, 0x02,  188 /* Public */,
     120,    2,  879,    4, 0x02,  192 /* Public */,
     121,    2,  884,    4, 0x02,  195 /* Public */,
     122,    1,  889,    4, 0x02,  198 /* Public */,
     123,    1,  892,    4, 0x02,  200 /* Public */,
     124,    0,  895,    4, 0x02,  202 /* Public */,
     125,    1,  896,    4, 0x02,  203 /* Public */,
     126,    2,  899,    4, 0x02,  205 /* Public */,
     128,    2,  904,    4, 0x102,  208 /* Public | MethodIsConst  */,
     129,    2,  909,    4, 0x102,  211 /* Public | MethodIsConst  */,
     130,    3,  914,    4, 0x02,  214 /* Public */,
     132,    0,  921,    4, 0x02,  218 /* Public */,
     133,    1,  922,    4, 0x02,  219 /* Public */,
     134,    1,  925,    4, 0x102,  221 /* Public | MethodIsConst  */,
     135,    1,  928,    4, 0x102,  223 /* Public | MethodIsConst  */,
     136,    2,  931,    4, 0x02,  225 /* Public */,
     139,    0,  936,    4, 0x02,  228 /* Public */,
     140,    2,  937,    4, 0x02,  229 /* Public */,
     140,    1,  942,    4, 0x22,  232 /* Public | MethodCloned */,
     141,    1,  945,    4, 0x02,  234 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString, QMetaType::Bool,    8,    9,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool, QMetaType::QString,   12,    8,
    QMetaType::Void, QMetaType::Bool, QMetaType::QString,   12,    8,
    QMetaType::Void, QMetaType::Bool, QMetaType::QString,   12,    8,
    QMetaType::Void, QMetaType::Bool, QMetaType::QString,   12,    8,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   25,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   29,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   25,

 // slots: parameters
    QMetaType::Void, QMetaType::Int, 0x80000000 | 36,   35,   37,
    QMetaType::Void,
    QMetaType::Void,

 // methods: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   46,
    QMetaType::Void, QMetaType::QString,   25,
    QMetaType::Void, QMetaType::QString,   25,
    QMetaType::Void,
    QMetaType::Int, QMetaType::QString,   25,
    QMetaType::Int, QMetaType::QString,   52,
    QMetaType::Int,
    QMetaType::Bool, QMetaType::QString, QMetaType::Bool, QMetaType::Int, QMetaType::QString, QMetaType::QString, QMetaType::QString,   54,   55,   56,   57,   58,   59,
    QMetaType::Bool, QMetaType::Int,   46,
    QMetaType::QVariantList, QMetaType::QString,   25,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   64,
    QMetaType::Void, QMetaType::Int, QMetaType::QString,   64,   66,
    QMetaType::QVariantList,
    QMetaType::QString, QMetaType::Int,   64,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   74,
    QMetaType::QStringList,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   78,
    QMetaType::QVariantMap, QMetaType::QString,   25,
    QMetaType::QVariantMap,
    QMetaType::QString,
    QMetaType::Bool, QMetaType::QString, QMetaType::Int, QMetaType::QString,   82,   83,   84,
    QMetaType::Bool, QMetaType::QString, QMetaType::Int,   82,   83,
    QMetaType::Bool, QMetaType::QString,   25,
    QMetaType::Bool, QMetaType::QString,   25,
    QMetaType::Bool, QMetaType::QString,   25,
    QMetaType::QVariantMap, QMetaType::QString,   25,
    QMetaType::Bool, QMetaType::QString, QMetaType::QString,   25,   66,
    QMetaType::Bool,
    QMetaType::QVariantMap, QMetaType::QString,   92,
    QMetaType::QVariantList, QMetaType::QString, QMetaType::QString,   92,   94,
    QMetaType::QVariantList, QMetaType::QString,   92,
    QMetaType::QVariantMap, QMetaType::QString, QMetaType::QVariantMap,   96,   97,
    QMetaType::QVariantMap, QMetaType::QVariantMap,   99,
    QMetaType::Bool, QMetaType::QVariantMap,   99,
    QMetaType::QStringList,
    QMetaType::QString, QMetaType::QString,  103,
    QMetaType::QString, QMetaType::QString,   25,
    QMetaType::Bool,
    QMetaType::Bool,
    QMetaType::Bool, QMetaType::QString, QMetaType::Bool, QMetaType::Int, QMetaType::Int, QMetaType::QString, QMetaType::QString, QMetaType::QString,   54,   55,  108,   56,   57,   58,   59,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   25,
    QMetaType::QString,
    QMetaType::QString,
    QMetaType::Bool, QMetaType::Int,   46,
    QMetaType::Bool, QMetaType::Int, QMetaType::Int,  116,  117,
    QMetaType::Void, QMetaType::Int, QMetaType::Int, QMetaType::QString,  116,  117,  119,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   25,  119,
    QMetaType::Void, QMetaType::QString, QMetaType::Int,   25,   46,
    QMetaType::Void, QMetaType::QString,  119,
    QMetaType::Bool, QMetaType::Int,   46,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   25,
    QMetaType::Bool, QMetaType::Int, QMetaType::Int,  116,  127,
    QMetaType::Bool, QMetaType::QString, QMetaType::Int,   25,   46,
    QMetaType::Bool, QMetaType::QString, QMetaType::QString,   25,  119,
    QMetaType::Bool, QMetaType::QString, QMetaType::Int, QMetaType::QString,   25,   46,  131,
    QMetaType::QVariantList,
    QMetaType::QVariantList, QMetaType::QString,   25,
    QMetaType::QVariantMap, QMetaType::QString,   25,
    QMetaType::Bool, QMetaType::QString,   25,
    QMetaType::QVariantList, QMetaType::Int, QMetaType::Int,  137,  138,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString, QMetaType::Bool,    8,    9,
    QMetaType::Void, QMetaType::QString,    8,
    QMetaType::Void, QMetaType::QString,   29,

 // properties: name, type, flags
     142, QMetaType::Bool, 0x00015001, uint(0), 0,
     143, QMetaType::QString, 0x00015001, uint(0), 0,
     144, QMetaType::Int, 0x00015001, uint(1), 0,
     145, QMetaType::Bool, 0x00015001, uint(0), 0,
     146, QMetaType::QString, 0x00015001, uint(0), 0,
     147, QMetaType::QString, 0x00015001, uint(0), 0,
     148, QMetaType::QVariantList, 0x00015001, uint(2), 0,
     149, QMetaType::Bool, 0x00015001, uint(4), 0,
     150, QMetaType::Bool, 0x00015001, uint(10), 0,
     151, QMetaType::Bool, 0x00015001, uint(11), 0,
     152, QMetaType::QString, 0x00015001, uint(11), 0,
     153, QMetaType::QString, 0x00015001, uint(11), 0,
     154, QMetaType::Int, 0x00015001, uint(12), 0,
     155, QMetaType::Bool, 0x00015001, uint(12), 0,
     156, QMetaType::Int, 0x00015001, uint(13), 0,
     157, QMetaType::Int, 0x00015001, uint(14), 0,
     158, QMetaType::QString, 0x00015103, uint(15), 0,
     159, QMetaType::QVariantList, 0x00015001, uint(16), 0,
     160, QMetaType::QObjectStar, 0x00015401, uint(-1), 0,
     161, QMetaType::Bool, 0x00015001, uint(0), 0,
     162, QMetaType::QString, 0x00015001, uint(21), 0,
     163, QMetaType::QString, 0x00015001, uint(21), 0,
     164, QMetaType::QString, 0x00015001, uint(21), 0,
     165, QMetaType::Bool, 0x00015103, uint(22), 0,
     166, QMetaType::Bool, 0x00015103, uint(23), 0,
     167, QMetaType::Int, 0x00015103, uint(19), 0,
     168, QMetaType::QString, 0x00015001, uint(4), 0,
     169, QMetaType::Bool, 0x00015001, uint(0), 0,
     170, QMetaType::Int, 0x00015001, uint(0), 0,
     171, QMetaType::QVariantMap, 0x00015001, uint(0), 0,
     172, QMetaType::QVariantList, 0x00015001, uint(9), 0,
     173, QMetaType::QVariantList, 0x00015001, uint(9), 0,
     174, QMetaType::QVariantList, 0x00015001, uint(9), 0,
     175, QMetaType::QVariantList, 0x00015001, uint(9), 0,

       0        // eod
};

void VaultController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<VaultController *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->statusChanged(); break;
        case 1: _t->fingerprintsChanged(); break;
        case 2: _t->logsChanged(); break;
        case 3: _t->notificationReceived((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2]))); break;
        case 4: _t->setupRunningChanged(); break;
        case 5: _t->setupFinished((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 6: _t->loopCreateFinished((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 7: _t->deviceDeleteFinished((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 8: _t->storagePlanFinished((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 9: _t->devicesChanged(); break;
        case 10: _t->togglingChanged(); break;
        case 11: _t->fingerprintUnlockActiveChanged(); break;
        case 12: _t->fingerprintPromptStateChanged(); break;
        case 13: _t->fingerprintEnrollStepChanged(); break;
        case 14: _t->setupOwnerFingerIdChanged(); break;
        case 15: _t->setupOwnerLabelChanged(); break;
        case 16: _t->setupAccessEntriesChanged(); break;
        case 17: _t->deleteVaultGatePassed((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 18: _t->vaultAccessListChanged(); break;
        case 19: _t->autoLockTimeoutChanged(); break;
        case 20: _t->navigationRequested((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 21: _t->sensorStatusChanged(); break;
        case 22: _t->autoDetectUsbChanged(); break;
        case 23: _t->hardwareDisconnectLockdownChanged(); break;
        case 24: _t->lockRequiresForceConfirmation((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 25: _t->onSimFinished((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QProcess::ExitStatus>>(_a[2]))); break;
        case 26: _t->onSerialConnectionChanged(); break;
        case 27: _t->pollStatus(); break;
        case 28: _t->toggleVault(); break;
        case 29: _t->forceLockVault(); break;
        case 30: _t->beginFingerprintUnlock(); break;
        case 31: _t->cancelFingerprintUnlock(); break;
        case 32: _t->continueAfterDecision(); break;
        case 33: _t->beginSetupOwnerEnroll((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 34: _t->beginSetupOwnerEnrollForDevice((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 35: _t->prepareSetupOwnerSlot((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 36: _t->clearSetupWizardSession(); break;
        case 37: { int _r = _t->ownerFingerSlotForDevice((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 38: { int _r = _t->nextFreeFingerSlot((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 39: { int _r = _t->nextFreeFingerSlot();
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 40: { bool _r = _t->beginVaultSetupBiometric((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[6])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 41: { bool _r = _t->fingerInAccessList((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 42: { QVariantList _r = _t->accessListForDevice((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 43: _t->addFingerprint(); break;
        case 44: _t->removeFingerprint((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 45: _t->renameFingerprint((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 46: { QVariantList _r = _t->getStorageDevices();
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 47: { QString _r = _t->fingerprintLabel((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 48: _t->refreshFingerprints(); break;
        case 49: _t->startSimulator(); break;
        case 50: _t->stopSimulator(); break;
        case 51: _t->reconnectSerial(); break;
        case 52: _t->connectToConfiguredPort((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 53: { QStringList _r = _t->availableSerialPorts();
            if (_a[0]) *reinterpret_cast< QStringList*>(_a[0]) = std::move(_r); }  break;
        case 54: _t->clearSerialPortSettings(); break;
        case 55: _t->setSelectedDevice((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 56: { QVariantMap _r = _t->createSetupSession((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 57: { QVariantMap _r = _t->createSetupSession();
            if (_a[0]) *reinterpret_cast< QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 58: { QString _r = _t->defaultContainersDir();
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 59: { bool _r = _t->createLoopDevice((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 60: { bool _r = _t->createLoopDevice((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 61: { bool _r = _t->deleteDevice((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 62: { bool _r = _t->deleteVault((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 63: { bool _r = _t->deleteVaultRequiresBiometric((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 64: { QVariantMap _r = _t->previewDeleteVault((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 65: { bool _r = _t->renameVaultDisplayName((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 66: { bool _r = _t->reconcileVaultRuntime();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 67: { QVariantMap _r = _t->storageLayoutForDevice((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 68: { QVariantList _r = _t->availablePartitionActions((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 69: { QVariantList _r = _t->availablePartitionActions((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 70: { QVariantMap _r = _t->buildStoragePlan((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QVariantMap>>(_a[2])));
            if (_a[0]) *reinterpret_cast< QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 71: { QVariantMap _r = _t->previewStoragePlan((*reinterpret_cast< std::add_pointer_t<QVariantMap>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 72: { bool _r = _t->applyStoragePlan((*reinterpret_cast< std::add_pointer_t<QVariantMap>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 73: { QStringList _r = _t->supportedFilesystems();
            if (_a[0]) *reinterpret_cast< QStringList*>(_a[0]) = std::move(_r); }  break;
        case 74: { QString _r = _t->resolveManagedDiskPath((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 75: { QString _r = _t->registryBackingForDevice((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 76: { bool _r = _t->emergencyClose();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 77: { bool _r = _t->vaultRuntimeOpen();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 78: { bool _r = _t->setupVault((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[6])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[7])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 79: _t->openInFileManager(); break;
        case 80: _t->openInTerminal(); break;
        case 81: _t->navigateToVaultManagerDetail((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 82: { QString _r = _t->takePendingVaultManagerDetailPath();
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 83: { QString _r = _t->savedSerialPort();
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 84: { bool _r = _t->verifyOwner((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 85: { bool _r = _t->grantAccess((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 86: _t->beginGrantAccessEnroll((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 87: _t->beginGrantAccessForVault((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 88: _t->beginRevokeAccessForVault((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 89: _t->beginSetupAccessEnroll((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 90: { bool _r = _t->removePendingSetupAccess((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 91: _t->clearPendingSetupAccess(); break;
        case 92: _t->beginDeleteVaultBiometricGate((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 93: { bool _r = _t->revokeAccess((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 94: { bool _r = _t->isFingerAllowedForVault((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 95: { bool _r = _t->labelExistsForVault((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 96: { bool _r = _t->renameVaultFingerprintLabel((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 97: { QVariantList _r = _t->getKeyslots();
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 98: { QVariantList _r = _t->getKeyslotsForDevice((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 99: { QVariantMap _r = _t->vaultDetailForDevice((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 100: { bool _r = _t->isVaultUnlockedForDevice((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 101: { QVariantList _r = _t->getSecurityLogsPage((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 102: _t->clearLogs(); break;
        case 103: _t->notifyUser((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2]))); break;
        case 104: _t->notifyUser((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 105: _t->navigateToPage((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (VaultController::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VaultController::statusChanged)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (VaultController::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VaultController::fingerprintsChanged)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (VaultController::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VaultController::logsChanged)) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (VaultController::*)(const QString & , bool );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VaultController::notificationReceived)) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (VaultController::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VaultController::setupRunningChanged)) {
                *result = 4;
                return;
            }
        }
        {
            using _t = void (VaultController::*)(bool , QString );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VaultController::setupFinished)) {
                *result = 5;
                return;
            }
        }
        {
            using _t = void (VaultController::*)(bool , QString );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VaultController::loopCreateFinished)) {
                *result = 6;
                return;
            }
        }
        {
            using _t = void (VaultController::*)(bool , QString );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VaultController::deviceDeleteFinished)) {
                *result = 7;
                return;
            }
        }
        {
            using _t = void (VaultController::*)(bool , QString );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VaultController::storagePlanFinished)) {
                *result = 8;
                return;
            }
        }
        {
            using _t = void (VaultController::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VaultController::devicesChanged)) {
                *result = 9;
                return;
            }
        }
        {
            using _t = void (VaultController::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VaultController::togglingChanged)) {
                *result = 10;
                return;
            }
        }
        {
            using _t = void (VaultController::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VaultController::fingerprintUnlockActiveChanged)) {
                *result = 11;
                return;
            }
        }
        {
            using _t = void (VaultController::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VaultController::fingerprintPromptStateChanged)) {
                *result = 12;
                return;
            }
        }
        {
            using _t = void (VaultController::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VaultController::fingerprintEnrollStepChanged)) {
                *result = 13;
                return;
            }
        }
        {
            using _t = void (VaultController::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VaultController::setupOwnerFingerIdChanged)) {
                *result = 14;
                return;
            }
        }
        {
            using _t = void (VaultController::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VaultController::setupOwnerLabelChanged)) {
                *result = 15;
                return;
            }
        }
        {
            using _t = void (VaultController::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VaultController::setupAccessEntriesChanged)) {
                *result = 16;
                return;
            }
        }
        {
            using _t = void (VaultController::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VaultController::deleteVaultGatePassed)) {
                *result = 17;
                return;
            }
        }
        {
            using _t = void (VaultController::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VaultController::vaultAccessListChanged)) {
                *result = 18;
                return;
            }
        }
        {
            using _t = void (VaultController::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VaultController::autoLockTimeoutChanged)) {
                *result = 19;
                return;
            }
        }
        {
            using _t = void (VaultController::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VaultController::navigationRequested)) {
                *result = 20;
                return;
            }
        }
        {
            using _t = void (VaultController::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VaultController::sensorStatusChanged)) {
                *result = 21;
                return;
            }
        }
        {
            using _t = void (VaultController::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VaultController::autoDetectUsbChanged)) {
                *result = 22;
                return;
            }
        }
        {
            using _t = void (VaultController::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VaultController::hardwareDisconnectLockdownChanged)) {
                *result = 23;
                return;
            }
        }
        {
            using _t = void (VaultController::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VaultController::lockRequiresForceConfirmation)) {
                *result = 24;
                return;
            }
        }
    }
#ifndef QT_NO_PROPERTIES
    else if (_c == QMetaObject::ReadProperty) {
        auto *_t = static_cast<VaultController *>(_o);
        (void)_t;
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< bool*>(_v) = _t->isLocked(); break;
        case 1: *reinterpret_cast< QString*>(_v) = _t->vaultStatusText(); break;
        case 2: *reinterpret_cast< int*>(_v) = _t->fingerprintCount(); break;
        case 3: *reinterpret_cast< bool*>(_v) = _t->simulatorRunning(); break;
        case 4: *reinterpret_cast< QString*>(_v) = _t->selectedDevice(); break;
        case 5: *reinterpret_cast< QString*>(_v) = _t->unlockedDevice(); break;
        case 6: *reinterpret_cast< QVariantList*>(_v) = _t->securityLogs(); break;
        case 7: *reinterpret_cast< bool*>(_v) = _t->isSetupRunning(); break;
        case 8: *reinterpret_cast< bool*>(_v) = _t->isToggling(); break;
        case 9: *reinterpret_cast< bool*>(_v) = _t->fingerprintUnlockActive(); break;
        case 10: *reinterpret_cast< QString*>(_v) = _t->fingerprintUnlockMessage(); break;
        case 11: *reinterpret_cast< QString*>(_v) = _t->fingerprintFlow(); break;
        case 12: *reinterpret_cast< int*>(_v) = _t->fingerprintPromptState(); break;
        case 13: *reinterpret_cast< bool*>(_v) = _t->fingerprintAwaitingDecision(); break;
        case 14: *reinterpret_cast< int*>(_v) = _t->fingerprintEnrollStep(); break;
        case 15: *reinterpret_cast< int*>(_v) = _t->setupOwnerFingerId(); break;
        case 16: *reinterpret_cast< QString*>(_v) = _t->setupOwnerLabel(); break;
        case 17: *reinterpret_cast< QVariantList*>(_v) = _t->setupAccessEntries(); break;
        case 18: *reinterpret_cast< QObject**>(_v) = _t->serialBridgeObject(); break;
        case 19: *reinterpret_cast< bool*>(_v) = _t->sensorConnected(); break;
        case 20: *reinterpret_cast< QString*>(_v) = _t->sensorStatus(); break;
        case 21: *reinterpret_cast< QString*>(_v) = _t->sensorStatusText(); break;
        case 22: *reinterpret_cast< QString*>(_v) = _t->sensorPortPath(); break;
        case 23: *reinterpret_cast< bool*>(_v) = _t->autoDetectUsb(); break;
        case 24: *reinterpret_cast< bool*>(_v) = _t->hardwareDisconnectLockdown(); break;
        case 25: *reinterpret_cast< int*>(_v) = _t->autoLockTimeout(); break;
        case 26: *reinterpret_cast< QString*>(_v) = _t->setupStatusText(); break;
        case 27: *reinterpret_cast< bool*>(_v) = _t->isSelectedDeviceVault(); break;
        case 28: *reinterpret_cast< int*>(_v) = _t->selectedDeviceCapacityMB(); break;
        case 29: *reinterpret_cast< QVariantMap*>(_v) = _t->selectedDeviceInfo(); break;
        case 30: *reinterpret_cast< QVariantList*>(_v) = _t->storageDevices(); break;
        case 31: *reinterpret_cast< QVariantList*>(_v) = _t->registeredVaults(); break;
        case 32: *reinterpret_cast< QVariantList*>(_v) = _t->managedDevices(); break;
        case 33: *reinterpret_cast< QVariantList*>(_v) = _t->discoveredStorageDevices(); break;
        default: break;
        }
    } else if (_c == QMetaObject::WriteProperty) {
        auto *_t = static_cast<VaultController *>(_o);
        (void)_t;
        void *_v = _a[0];
        switch (_id) {
        case 16: _t->setSetupOwnerLabel(*reinterpret_cast< QString*>(_v)); break;
        case 23: _t->setAutoDetectUsb(*reinterpret_cast< bool*>(_v)); break;
        case 24: _t->setHardwareDisconnectLockdown(*reinterpret_cast< bool*>(_v)); break;
        case 25: _t->setAutoLockTimeout(*reinterpret_cast< int*>(_v)); break;
        default: break;
        }
    } else if (_c == QMetaObject::ResetProperty) {
    } else if (_c == QMetaObject::BindableProperty) {
    }
#endif // QT_NO_PROPERTIES
}

const QMetaObject VaultController::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_VaultController.offsetsAndSize,
    qt_meta_data_VaultController,
    qt_static_metacall,
    nullptr,
qt_metaTypeArray<
bool, QString, int, bool, QString, QString, QVariantList, bool, bool, bool, QString, QString, int, bool, int, int, QString, QVariantList, QObject*, bool, QString, QString, QString, bool, bool, int, QString, bool, int, QVariantMap, QVariantList, QVariantList, QVariantList, QVariantList, VaultController, void, void, void, void, const QString &, bool, void, void, bool, QString, void, bool, QString, void, bool, QString, void, bool, QString, void, void, void, void, void, void, void, void, void, const QString &, void, void, void, const QString &, void, void, void, void, const QString &
, void, int, QProcess::ExitStatus, void, void
, void, void, void, void, void, void, int, void, const QString &, void, const QString &, void, int, const QString &, int, const QString &, int, bool, const QString &, bool, int, const QString &, const QString &, const QString &, bool, int, QVariantList, const QString &, void, void, int, void, int, const QString &, QVariantList, QString, int, void, void, void, void, void, const QString &, QStringList, void, void, const QString &, QVariantMap, const QString &, QVariantMap, QString, bool, const QString &, int, const QString &, bool, const QString &, int, bool, const QString &, bool, const QString &, bool, const QString &, QVariantMap, const QString &, bool, const QString &, const QString &, bool, QVariantMap, const QString &, QVariantList, const QString &, const QString &, QVariantList, const QString &, QVariantMap, const QString &, const QVariantMap &, QVariantMap, const QVariantMap &, bool, const QVariantMap &, QStringList, QString, const QString &, QString, const QString &, bool, bool, bool, const QString &, bool, int, int, const QString &, const QString &, const QString &, void, void, void, const QString &, QString, QString, bool, int, bool, int, int, void, int, int, const QString &, void, const QString &, const QString &, void, const QString &, int, void, const QString &, bool, int, void, void, const QString &, bool, int, int, bool, const QString &, int, bool, const QString &, const QString &, bool, const QString &, int, const QString &, QVariantList, QVariantList, const QString &, QVariantMap, const QString &, bool, const QString &, QVariantList, int, int, void, void, const QString &, bool, void, const QString &, void, const QString &

>,
    nullptr
} };


const QMetaObject *VaultController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *VaultController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_VaultController.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int VaultController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 106)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 106;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 106)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 106;
    }
#ifndef QT_NO_PROPERTIES
    else if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 34;
    }
#endif // QT_NO_PROPERTIES
    return _id;
}

// SIGNAL 0
void VaultController::statusChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void VaultController::fingerprintsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void VaultController::logsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void VaultController::notificationReceived(const QString & _t1, bool _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void VaultController::setupRunningChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void VaultController::setupFinished(bool _t1, QString _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}

// SIGNAL 6
void VaultController::loopCreateFinished(bool _t1, QString _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 6, _a);
}

// SIGNAL 7
void VaultController::deviceDeleteFinished(bool _t1, QString _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 7, _a);
}

// SIGNAL 8
void VaultController::storagePlanFinished(bool _t1, QString _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 8, _a);
}

// SIGNAL 9
void VaultController::devicesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 9, nullptr);
}

// SIGNAL 10
void VaultController::togglingChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 10, nullptr);
}

// SIGNAL 11
void VaultController::fingerprintUnlockActiveChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 11, nullptr);
}

// SIGNAL 12
void VaultController::fingerprintPromptStateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 12, nullptr);
}

// SIGNAL 13
void VaultController::fingerprintEnrollStepChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 13, nullptr);
}

// SIGNAL 14
void VaultController::setupOwnerFingerIdChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 14, nullptr);
}

// SIGNAL 15
void VaultController::setupOwnerLabelChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 15, nullptr);
}

// SIGNAL 16
void VaultController::setupAccessEntriesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 16, nullptr);
}

// SIGNAL 17
void VaultController::deleteVaultGatePassed(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 17, _a);
}

// SIGNAL 18
void VaultController::vaultAccessListChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 18, nullptr);
}

// SIGNAL 19
void VaultController::autoLockTimeoutChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 19, nullptr);
}

// SIGNAL 20
void VaultController::navigationRequested(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 20, _a);
}

// SIGNAL 21
void VaultController::sensorStatusChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 21, nullptr);
}

// SIGNAL 22
void VaultController::autoDetectUsbChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 22, nullptr);
}

// SIGNAL 23
void VaultController::hardwareDisconnectLockdownChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 23, nullptr);
}

// SIGNAL 24
void VaultController::lockRequiresForceConfirmation(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 24, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
