import QtQuick
import QtQuick.Controls
import DigitalVault

Item {
    id: root
    width: parent ? parent.width : 0
    height: parent ? parent.height : 0

    property string devicePath: ""
    property var layoutData: ({})
    property string selectedPartitionPath: ""
    property int detailTabIndex: 0

    signal backRequested()
    signal partitionPathChanged(string path)
    signal renameRequested()
    signal deleteRequested()
    signal setupEncryptionRequested()

    function deviceEntry() {
        const devices = vaultController.managedDevices
        for (let i = 0; i < devices.length; ++i) {
            if (devices[i].path === devicePath)
                return devices[i]
        }
        return {}
    }

    readonly property var deviceData: deviceEntry()

    onDevicePathChanged: {
        detailTabIndex = 0
        root.partitionPathChanged("")
        if (accessCard)
            accessCard.refreshAccessList()
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        Column {
            width: parent.width - 24
            spacing: 20
            leftPadding: 12
            rightPadding: 12
            topPadding: 4
            bottomPadding: 24

            VaultManagerNavBar {
                width: parent.width
                title: deviceData.displayName || deviceData.name || devicePath || "Vault"
                subtitle: devicePath
                onBackRequested: root.backRequested()
            }

            Text {
                width: parent.width
                text: "Vault Manager / "
                      + (deviceData.displayName || deviceData.name || "Vault")
                      + " / "
                      + (root.detailTabIndex === 0 ? "Layout" : "Access")
                color: Theme.textMuted
                font { pixelSize: 13; family: Theme.fontFamily }
                elide: Text.ElideRight
            }

            Rectangle {
                width: parent.width
                radius: Theme.radius
                color: Theme.bgCard
                border.color: Theme.border
                border.width: 1
                implicitHeight: summaryCol.implicitHeight + 28

                Column {
                    id: summaryCol
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    Item {
                        width: parent.width
                        height: 30

                        Row {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 12

                            Rectangle {
                                height: 24
                                width: summaryStatus.implicitWidth + 16
                                radius: 12
                                color: {
                                    const s = deviceData.statusLabel || ""
                                    if (s === "Unlocked") return Theme.successBg
                                    if (s === "Unavailable") return Theme.dangerBg
                                    return Theme.bgOverlay
                                }
                                Text {
                                    id: summaryStatus
                                    anchors.centerIn: parent
                                    text: deviceData.statusLabel || "Locked"
                                    color: {
                                        const s = deviceData.statusLabel || ""
                                        if (s === "Unlocked") return Theme.success
                                        if (s === "Unavailable") return Theme.danger
                                        return Theme.textSecondary
                                    }
                                    font { pixelSize: 12; bold: true; family: Theme.fontFamily }
                                }
                            }

                            Text {
                                id: dashLink
                                anchors.verticalCenter: parent.verticalCenter
                                text: "Unlock on Dashboard →"
                                color: Theme.accent
                                font { pixelSize: 13; family: Theme.fontFamily }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: vaultController.navigateToPage("Dashboard.qml")
                                }
                            }
                        }

                        Rectangle {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            width: renameLbl.implicitWidth + 20
                            height: 30
                            radius: 15
                            color: renameMouse.containsMouse ? Theme.bgHover : Theme.bgOverlay
                            border.color: Theme.border

                            Text {
                                id: renameLbl
                                anchors.centerIn: parent
                                text: "Rename"
                                color: Theme.textPrimary
                                font { pixelSize: 13; family: Theme.fontFamily }
                            }

                            MouseArea {
                                id: renameMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.renameRequested()
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: deviceData.description || ""
                        visible: text !== ""
                        color: Theme.textSecondary
                        font { pixelSize: 13; family: Theme.fontFamily }
                    }
                }
            }

            Rectangle {
                id: detailTabs
                width: parent.width
                height: 76
                radius: Theme.radius
                color: Theme.bgCard
                border.color: Theme.border
                border.width: 1

                Rectangle {
                    anchors {
                        left: parent.left
                        right: parent.right
                        top: parent.top
                        leftMargin: 1
                        rightMargin: 1
                        topMargin: 42
                    }
                    height: 1
                    color: Theme.border
                }

                Row {
                    anchors {
                        left: parent.left
                        right: parent.right
                        top: parent.top
                        leftMargin: 8
                        rightMargin: 8
                        topMargin: 6
                    }
                    height: 38
                    spacing: 0

                    Repeater {
                        model: [
                            {
                                label: "Layout",
                                meta: (root.layoutData.partitions || []).length + " partitions",
                                index: 0
                            },
                            {
                                label: "Access",
                                meta: accessCard.accessEntries.length + "/4 people",
                                index: 1
                            }
                        ]

                        delegate: Rectangle {
                            id: tabItem
                            width: parent.width / 2
                            height: parent.height
                            radius: 0
                            color: root.detailTabIndex === modelData.index
                                   ? Theme.bgSurface
                                   : (tabMouse.containsMouse ? Theme.bgHover : "transparent")

                            Row {
                                anchors.centerIn: parent
                                spacing: 8

                                Text {
                                    text: modelData.label
                                    color: root.detailTabIndex === modelData.index
                                           ? Theme.navActiveText
                                           : Theme.textPrimary
                                    font { pixelSize: 12; bold: true; family: Theme.fontFamily }
                                }

                                Text {
                                    text: modelData.meta
                                    color: Theme.textMuted
                                    font { pixelSize: 12; family: Theme.fontFamily }
                                }
                            }

                            Rectangle {
                                anchors {
                                    left: parent.left
                                    right: parent.right
                                    bottom: parent.bottom
                                    leftMargin: 12
                                    rightMargin: 12
                                }
                                height: 3
                                radius: 2
                                color: Theme.accent
                                visible: root.detailTabIndex === modelData.index
                            }

                            MouseArea {
                                id: tabMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.detailTabIndex = modelData.index
                            }
                        }
                    }
                }

                Text {
                    anchors {
                        left: parent.left
                        right: parent.right
                        bottom: parent.bottom
                        leftMargin: 14
                        rightMargin: 14
                        bottomMargin: 8
                    }
                    text: root.detailTabIndex === 0
                          ? "Partition layout and storage actions for this vault."
                          : "Fingerprints allowed to unlock this vault."
                    color: Theme.textSecondary
                    font { pixelSize: 13; family: Theme.fontFamily }
                    elide: Text.ElideRight
                }
            }

            Rectangle {
                id: accessCard
                width: parent.width
                visible: root.detailTabIndex === 1
                radius: Theme.radius
                color: Theme.bgCard
                border.color: Theme.border
                border.width: 1
                implicitHeight: accessCol.implicitHeight + 28

                property var accessEntries: []

                readonly property bool canUseSensor: vaultController.sensorConnected
                    || (vaultController.serialBridge && vaultController.serialBridge.connected)
                readonly property bool fpActive: vaultController.fingerprintUnlockActive
                readonly property bool atAccessCapacity: accessEntries.length >= 4
                readonly property bool canManageAccess: false

                function refreshAccessList() {
                    if (!root.devicePath) {
                        accessEntries = []
                        return
                    }
                    accessEntries = vaultController.accessListForDevice(root.devicePath)
                }

                function submitGrantAccess() {
                    const label = grantLabelField.text.trim()
                    if (!label) {
                        vaultController.notifyUser(
                            "Enter a fingerprint name.", true)
                        return
                    }
                    if (vaultController.labelExistsForVault(root.devicePath, label)) {
                        vaultController.notifyUser(
                            'Display name "' + label + '" is already used for this vault.', true)
                        return
                    }
                    vaultController.beginGrantAccessForVault(root.devicePath, label)
                }

                function renameAccess(fingerId, label) {
                    const trimmed = label.trim()
                    if (!trimmed) {
                        vaultController.notifyUser("Enter a fingerprint name.", true)
                        return false
                    }
                    return vaultController.renameVaultFingerprintLabel(root.devicePath, fingerId, trimmed)
                }

                Component.onCompleted: refreshAccessList()

                Connections {
                    target: vaultController
                    function onVaultAccessListChanged() { accessCard.refreshAccessList() }
                    function onDevicesChanged() { accessCard.refreshAccessList() }
                }

                Column {
                    id: accessCol
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 12

                    Text {
                        text: "People who can unlock this vault"
                        color: Theme.textPrimary
                        font { pixelSize: 17; bold: true; family: Theme.fontFamily }
                    }

                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: "Access was configured during setup and cannot be changed later."
                        color: Theme.textSecondary
                        font { pixelSize: 13; family: Theme.fontFamily }
                    }

                    Text {
                        width: parent.width
                        visible: accessCard.accessEntries.length === 0
                        wrapMode: Text.Wrap
                        text: "No additional fingerprints. Owner access was created during setup."
                        color: Theme.textMuted
                        font { pixelSize: 13; family: Theme.fontFamily }
                    }

                    Repeater {
                        model: accessCard.accessEntries
                        delegate: Rectangle {
                            id: accessRow
                            width: parent.width
                            height: 54
                            radius: 8
                            color: Theme.bgOverlay
                            border.color: Theme.border
                            border.width: 1
                            property bool editing: false

                            Row {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 8
                                spacing: 8

                                Item {
                                    width: Math.max(0, parent.width - actionGroup.width - 8)
                                    height: parent.height

                                    Text {
                                        visible: !accessRow.editing
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: parent.width
                                        text: modelData.label || "Unnamed fingerprint"
                                        color: Theme.textPrimary
                                        font { pixelSize: 15; bold: true; family: Theme.fontFamily }
                                        elide: Text.ElideRight
                                    }

                                    TextInput {
                                        id: renameInput
                                        visible: accessRow.editing
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: parent.width
                                        height: 34
                                        text: modelData.label || ""
                                        color: Theme.textPrimary
                                        selectionColor: Theme.accent
                                        selectedTextColor: "white"
                                        font { pixelSize: 15; family: Theme.fontFamily }
                                        verticalAlignment: TextInput.AlignVCenter
                                        clip: true
                                        onAccepted: {
                                            if (accessCard.renameAccess(modelData.fingerId, text))
                                                accessRow.editing = false
                                        }

                                        Rectangle {
                                            anchors.fill: parent
                                            anchors.margins: -8
                                            z: -1
                                            radius: 8
                                            color: Theme.bgInput
                                            border.color: renameInput.activeFocus ? Theme.accent : Theme.border
                                            border.width: 1
                                        }
                                    }
                                }

                                Row {
                                    id: actionGroup
                                        width: modelData.isOwner ? 132 : 96
                                    height: parent.height
                                    spacing: 6
                                    layoutDirection: Qt.RightToLeft

                                    Rectangle {
                                        visible: accessRow.editing
                                        width: saveLbl.implicitWidth + 16
                                        height: 32
                                        radius: Theme.radiusSm
                                        color: saveMouse.containsMouse ? Theme.accentGlow : "transparent"

                                        Text {
                                            id: saveLbl
                                            anchors.centerIn: parent
                                            text: "Save"
                                            color: Theme.accent
                                            font { pixelSize: 13; bold: true; family: Theme.fontFamily }
                                        }

                                        MouseArea {
                                            id: saveMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            enabled: accessCard.canManageAccess
                                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                            onClicked: {
                                                if (accessCard.renameAccess(modelData.fingerId, renameInput.text))
                                                    accessRow.editing = false
                                            }
                                        }
                                    }

                                    Rectangle {
                                        visible: accessRow.editing
                                        width: cancelLbl.implicitWidth + 16
                                        height: 32
                                        radius: Theme.radiusSm
                                        color: cancelMouse.containsMouse ? Theme.bgHover : "transparent"

                                        Text {
                                            id: cancelLbl
                                            anchors.centerIn: parent
                                            text: "Cancel"
                                            color: Theme.textSecondary
                                            font { pixelSize: 13; bold: true; family: Theme.fontFamily }
                                        }

                                        MouseArea {
                                            id: cancelMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                renameInput.text = modelData.label || ""
                                                accessRow.editing = false
                                            }
                                        }
                                    }

                                    Rectangle {
                                        visible: false
                                        width: editLbl.implicitWidth + 16
                                        height: 32
                                        radius: Theme.radiusSm
                                        color: editMouse.containsMouse ? Theme.accentGlow : "transparent"

                                        Text {
                                            id: editLbl
                                            anchors.centerIn: parent
                                            text: "Rename"
                                            color: Theme.accent
                                            font { pixelSize: 13; bold: true; family: Theme.fontFamily }
                                        }

                                        MouseArea {
                                            id: editMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            enabled: accessCard.canManageAccess
                                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                            onClicked: {
                                                accessRow.editing = true
                                                renameInput.forceActiveFocus()
                                                renameInput.selectAll()
                                            }
                                        }
                                    }

                                    Rectangle {
                                        visible: false
                                        width: removeLbl.implicitWidth + 16
                                        height: 32
                                        radius: Theme.radiusSm
                                        color: removeMouse.containsMouse ? Theme.dangerBg : "transparent"

                                        Text {
                                            id: removeLbl
                                            anchors.centerIn: parent
                                            text: "Remove"
                                            color: Theme.danger
                                            font { pixelSize: 13; bold: true; family: Theme.fontFamily }
                                        }

                                        MouseArea {
                                            id: removeMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            enabled: accessCard.canManageAccess
                                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                            onClicked: vaultController.beginRevokeAccessForVault(
                                                root.devicePath, modelData.fingerId)
                                        }
                                    }

                                    Rectangle {
                                        visible: modelData.isOwner && !accessRow.editing
                                        width: ownerLbl.implicitWidth + 16
                                        height: 28
                                        radius: Theme.radiusSm
                                        color: Theme.accentGlow

                                        Text {
                                            id: ownerLbl
                                            anchors.centerIn: parent
                                            text: "Owner"
                                            color: Theme.accent
                                            font { pixelSize: 12; bold: true; family: Theme.fontFamily }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        visible: false
                        wrapMode: Text.Wrap
                        text: "Connect the fingerprint sensor to manage access."
                        color: Theme.textSecondary
                        font { pixelSize: 13; family: Theme.fontFamily }
                    }

                    Text {
                        visible: false
                        text: "Open Settings"
                        color: Theme.accent
                        font { pixelSize: 13; bold: true; family: Theme.fontFamily }
                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -4
                            cursorShape: Qt.PointingHandCursor
                            onClicked: vaultController.navigateToPage("Settings.qml")
                        }
                    }

                    Text {
                        width: parent.width
                        visible: false
                        text: "Maximum of 4 access entries for this vault."
                        color: Theme.warning
                        font { pixelSize: 13; family: Theme.fontFamily }
                    }

                    WizardTextField {
                        id: grantLabelField
                        width: parent.width
                        visible: false
                        label: "Name for new fingerprint"
                        placeholderText: "Left index finger"
                    }

                    WizardActionButton {
                        text: "Authorize fingerprint"
                        variant: "primary"
                        visible: false
                        enabled: accessCard.canManageAccess && !accessCard.atAccessCapacity
                        onClicked: accessCard.submitGrantAccess()
                    }
                }
            }

            Rectangle {
                id: layoutCard
                width: parent.width
                visible: root.detailTabIndex === 0
                radius: Theme.radius
                color: Theme.bgCard
                border.color: Theme.border
                border.width: 1
                implicitHeight: layoutCol.implicitHeight + 32

                Column {
                    id: layoutCol
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 14

                    Text {
                        text: "Layout editor"
                        color: Theme.textPrimary
                        font { pixelSize: 17; bold: true; family: Theme.fontFamily }
                    }

                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: "Select a partition from the visual layout or table to see available actions."
                        color: Theme.textSecondary
                        font { pixelSize: 13; family: Theme.fontFamily }
                    }

                    PartitionLayoutView {
                        width: parent.width
                        layoutData: root.layoutData
                        selectedPartitionPath: root.selectedPartitionPath
                        onPartitionSelected: function(path) {
                            root.partitionPathChanged(path)
                        }
                    }

                    PartitionLayoutTable {
                        width: parent.width
                        visible: (root.layoutData.partitions || []).length > 0
                        partitions: root.layoutData.partitions || []
                        selectedPartitionPath: root.selectedPartitionPath
                        onRowClicked: function(path) {
                            root.partitionPathChanged(path)
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: selectHintText.implicitHeight + 20
                        radius: Theme.radiusSm
                        visible: root.selectedPartitionPath === ""
                        color: Theme.bgOverlay
                        border.color: Theme.border
                        border.width: 1

                        Text {
                            id: selectHintText
                            anchors {
                                left: parent.left
                                right: parent.right
                                verticalCenter: parent.verticalCenter
                                leftMargin: 12
                                rightMargin: 12
                            }
                            wrapMode: Text.Wrap
                            text: "No partition selected. Choose a row or segment before running storage actions."
                            color: Theme.textSecondary
                            font { pixelSize: 13; family: Theme.fontFamily }
                        }
                    }

                    StorageActionPanel {
                        width: parent.width
                        actions: vaultController.availablePartitionActions(
                            root.devicePath, root.selectedPartitionPath)
                        onActionTriggered: function(actionId) {
                            if (actionId === "setup_encryption") {
                                root.setupEncryptionRequested()
                                return
                            }
                            if (actionId === "delete_vault") {
                                root.deleteRequested()
                                return
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: dangerZoneCol.implicitHeight + 24
                        radius: Theme.radius
                        color: Theme.dangerBg
                        border.color: Theme.danger
                        border.width: 1

                        Column {
                            id: dangerZoneCol
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 10

                            Text {
                                width: parent.width
                                text: "Danger zone"
                                color: Theme.danger
                                font { pixelSize: 12; bold: true; family: Theme.fontFamily }
                            }

                            Text {
                                width: parent.width
                                wrapMode: Text.Wrap
                                text: "Deleting a vault can remove encrypted data and registry entries. Review the confirmation dialog before continuing."
                                color: Theme.textPrimary
                                font { pixelSize: 13; family: Theme.fontFamily }
                            }

                            Rectangle {
                                width: deleteVaultLabel.implicitWidth + 24
                                height: 34
                                radius: Theme.radiusSm
                                color: delLayoutMouse.containsMouse ? Theme.bgSurface : "transparent"
                                border.color: Theme.danger
                                border.width: 1

                                Text {
                                    id: deleteVaultLabel
                                    anchors.centerIn: parent
                                    text: "Delete vault"
                                    color: Theme.danger
                                    font { pixelSize: 12; bold: true; family: Theme.fontFamily }
                                }

                                MouseArea {
                                    id: delLayoutMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.deleteRequested()
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
