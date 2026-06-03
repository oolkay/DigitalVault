import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DigitalVault

Rectangle {
    id: root
    radius: Theme.radius
    color: Theme.bgCard
    border.color: Theme.border
    border.width: 1

    property string devicePath: ""
    property var detail: ({})
    property string pendingForceLockDevice: ""

    function refreshDetail() {
        if (!devicePath)
            detail = {}
        else
            detail = vaultController.vaultDetailForDevice(devicePath)
    }

    onDevicePathChanged: refreshDetail()

    Connections {
        target: vaultController
        function onStatusChanged() { root.refreshDetail() }
        function onDevicesChanged() { root.refreshDetail() }
        function onLockRequiresForceConfirmation(devicePath) {
            if (devicePath !== root.devicePath)
                return
            root.pendingForceLockDevice = devicePath
            forceLockOverlay.visible = true
        }
    }

    ColumnLayout {
        id: panelLayout
        anchors.fill: parent
        anchors.margins: 28
        spacing: 20

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    text: detail.displayName || detail.name || "Vault"
                    color: Theme.textPrimary
                    font { pixelSize: 22; bold: true; family: Theme.fontFamily }
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                }

                Text {
                    text: {
                        var sz = detail.sizeLabel || ""
                        return sz ? (sz + " encrypted partition") : "Encrypted vault"
                    }
                    color: Theme.textSecondary
                    font { pixelSize: 13; family: Theme.fontFamily }
                }

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: detail.encryptionLabel
                           ? ("Encryption: " + detail.encryptionLabel)
                           : "Encryption: AES-256-XTS (LUKS2)"
                    color: Theme.textSecondary
                    font { pixelSize: 12; family: Theme.fontFamily }
                }

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: "This vault is encrypted with AES-256-XTS in LUKS2 mode."
                    color: Theme.textMuted
                    font { pixelSize: 11; family: Theme.fontFamily }
                }

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: {
                        var p = detail.devicePath || detail.path || devicePath || ""
                        return p ? ("Device: " + p) : ""
                    }
                    color: Theme.textMuted
                    font { pixelSize: 11; family: Theme.monoFamily }
                }
            }

            RowLayout {
                spacing: 10
                Layout.alignment: Qt.AlignTop | Qt.AlignRight

                Rectangle {
                    Layout.preferredWidth: editLabel.implicitWidth + 28
                    Layout.preferredHeight: 34
                    radius: 17
                    color: editMouse.containsMouse ? Theme.bgHover : Theme.bgOverlay
                    border.color: Theme.border
                    border.width: 1

                    Text {
                        id: editLabel
                        anchors.centerIn: parent
                        text: "Edit"
                        color: Theme.textPrimary
                        font { pixelSize: 12; bold: true; family: Theme.fontFamily }
                    }

                    MouseArea {
                        id: editMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: vaultController.navigateToVaultManagerDetail(devicePath)
                    }
                }

                VaultStatusBadge {
                    isUnlocked: detail.isUnlocked === true
                    isActiveVault: vaultController.isToggling
                                   && (devicePath === vaultController.unlockedDevice
                                       || (devicePath === vaultController.selectedDevice
                                           && !detail.isUnlocked))
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: stateMessage.implicitHeight + 22
            radius: Theme.radiusSm
            color: {
                if (vaultController.isToggling
                    && (devicePath === vaultController.unlockedDevice
                        || devicePath === vaultController.selectedDevice))
                    return Theme.warningBg
                return detail.isUnlocked ? Theme.successBg : Theme.bgOverlay
            }
            border.color: {
                if (vaultController.isToggling
                    && (devicePath === vaultController.unlockedDevice
                        || devicePath === vaultController.selectedDevice))
                    return Theme.warning
                return detail.isUnlocked ? Theme.success : Theme.border
            }
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 11
                spacing: 10

                Rectangle {
                    Layout.preferredWidth: 9
                    Layout.preferredHeight: 9
                    radius: 5
                    color: {
                        if (vaultController.isToggling
                            && (devicePath === vaultController.unlockedDevice
                                || devicePath === vaultController.selectedDevice))
                            return Theme.warning
                        return detail.isUnlocked ? Theme.success : Theme.textMuted
                    }
                }

                Text {
                    id: stateMessage
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: {
                        if (vaultController.isToggling
                            && (devicePath === vaultController.unlockedDevice
                                || devicePath === vaultController.selectedDevice))
                            return detail.isUnlocked
                                ? "Locking the vault. Keep this window open until the operation finishes."
                                : "Verifying access and decrypting the vault."
                        return detail.isUnlocked
                            ? "Files are accessible. Lock the vault when you are finished."
                            : "Vault is locked. Unlock requires fingerprint verification."
                    }
                    color: Theme.textPrimary
                    font { pixelSize: 12; family: Theme.fontFamily }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "Authorized access"
                color: Theme.textMuted
                font { pixelSize: 10; bold: true; family: Theme.fontFamily; letterSpacing: 1 }
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: "People who can unlock this vault."
                color: Theme.textSecondary
                font { pixelSize: 11; family: Theme.fontFamily }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: {
                        var n = detail.accessCount !== undefined ? detail.accessCount : 0
                        return n === 1 ? "1 person can unlock" : (n + " people can unlock")
                    }
                    color: Theme.textPrimary
                    font { pixelSize: 14; family: Theme.fontFamily }
                    Layout.fillWidth: true
                }

                Text {
                    text: accessListExpanded ? "Hide list" : "Show list"
                    color: Theme.accent
                    font { pixelSize: 12; underline: true; family: Theme.fontFamily }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: accessListExpanded = !accessListExpanded
                    }
                }
            }

            ScrollView {
                Layout.fillWidth: true
                visible: accessListExpanded && detail.accessList && detail.accessList.length > 0
                Layout.preferredHeight: Math.min(accessListCol.implicitHeight, 126)
                contentWidth: availableWidth
                clip: true

                Column {
                    id: accessListCol
                    width: parent.width
                    spacing: 6

                    Repeater {
                        model: detail.accessList || []
                        delegate: Rectangle {
                            width: accessListCol.width
                            height: 36
                            radius: 8
                            color: Theme.bgOverlay
                            border.color: Theme.border
                            border.width: 1

                            Row {
                                anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                                spacing: 10
                                Text {
                                    width: 44
                                    text: modelData.isOwner ? "OWNER" : "USER"
                                    color: modelData.isOwner ? Theme.success : Theme.textSecondary
                                    font { pixelSize: 9; bold: true; family: Theme.fontFamily }
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    width: Math.max(0, parent.width - 54)
                                    text: modelData.label || "Unnamed fingerprint"
                                    color: Theme.textPrimary
                                    font { pixelSize: 12; family: Theme.fontFamily }
                                    anchors.verticalCenter: parent.verticalCenter
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }
            }

            Text {
                visible: accessListExpanded && (!detail.accessList || detail.accessList.length === 0)
                text: "No one in the access list yet."
                color: Theme.textMuted
                font { pixelSize: 11; family: Theme.fontFamily }
            }

        }

        Item { Layout.fillHeight: true }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                radius: Theme.radiusSm
                color: {
                    if (vaultController.isToggling
                        && (devicePath === vaultController.unlockedDevice
                            || devicePath === vaultController.selectedDevice))
                        return Theme.warning
                    return detail.isUnlocked ? Theme.danger : Theme.success
                }
                opacity: devicePath ? 1 : 0.5

                Text {
                    anchors.centerIn: parent
                    text: {
                        if (vaultController.isToggling
                            && (devicePath === vaultController.unlockedDevice
                                || devicePath === vaultController.selectedDevice))
                            return detail.isUnlocked ? "Locking..." : "Decrypting..."
                        return detail.isUnlocked ? "Lock Vault" : "Unlock Vault"
                    }
                    color: "white"
                    font { pixelSize: 13; bold: true; family: Theme.fontFamily }
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: devicePath && !vaultController.isToggling
                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: {
                        if (vaultController.selectedDevice !== devicePath)
                            vaultController.setSelectedDevice(devicePath)
                        vaultController.toggleVault()
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                visible: detail.isUnlocked === true

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    radius: Theme.radiusSm
                    color: filesMouse.containsMouse ? Theme.accentHover : Theme.accent
                    Text {
                        anchors.centerIn: parent
                        text: "Open Files"
                        color: "white"
                        font { pixelSize: 12; bold: true; family: Theme.fontFamily }
                    }
                    MouseArea {
                        id: filesMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: vaultController.openInFileManager()
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    radius: Theme.radiusSm
                    color: termMouse.containsMouse ? Theme.bgHover : Theme.bgOverlay
                    border.color: Theme.border
                    border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text: "Terminal"
                        color: Theme.textPrimary
                        font { pixelSize: 12; bold: true; family: Theme.fontFamily }
                    }
                    MouseArea {
                        id: termMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: vaultController.openInTerminal()
                    }
                }
            }
        }
    }

    property bool accessListExpanded: false

    Rectangle {
        id: forceLockOverlay
        anchors.fill: parent
        z: 20
        visible: false
        color: Qt.rgba(0, 0, 0, 0.38)
        radius: root.radius

        function closeOverlay() {
            visible = false
            root.pendingForceLockDevice = ""
        }

        MouseArea {
            anchors.fill: parent
            onClicked: forceLockOverlay.closeOverlay()
        }

        Rectangle {
            id: forceLockCard
            width: Math.max(300, Math.min(parent.width - 36, 440))
            height: forceLockContent.implicitHeight + 36
            anchors.centerIn: parent
            radius: Theme.radius
            color: Theme.bgSurface
            border.color: Theme.border
            border.width: 1

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
            }

            ColumnLayout {
                id: forceLockContent
                anchors.fill: parent
                anchors.margins: 18
                spacing: 14

                Text {
                    Layout.fillWidth: true
                    text: "Force Lock Vault?"
                    color: Theme.warning
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    font { pixelSize: 16; bold: true; family: Theme.fontFamily }
                }

                Text {
                    Layout.fillWidth: true
                    text: "This vault is currently being used by another app or terminal."
                    color: Theme.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    font { pixelSize: 13; family: Theme.fontFamily }
                }

                Text {
                    Layout.fillWidth: true
                    text: "Force locking can close those processes. Unsaved work inside the vault may be lost."
                    color: Theme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    font { pixelSize: 12; family: Theme.fontFamily }
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 12

                    Rectangle {
                        Layout.preferredWidth: 112
                        Layout.preferredHeight: 36
                        radius: Theme.radiusSm
                        color: cancelForceMouse.containsMouse ? Theme.bgHover : Theme.bgOverlay
                        border.color: Theme.border
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: "Cancel"
                            color: Theme.textSecondary
                            font { pixelSize: 12; bold: true; family: Theme.fontFamily }
                        }

                        MouseArea {
                            id: cancelForceMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: forceLockOverlay.closeOverlay()
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 132
                        Layout.preferredHeight: 36
                        radius: Theme.radiusSm
                        color: confirmForceMouse.containsMouse ? Theme.warning : Theme.danger

                        Text {
                            anchors.centerIn: parent
                            text: "Force Lock"
                            color: "white"
                            font { pixelSize: 12; bold: true; family: Theme.fontFamily }
                        }

                        MouseArea {
                            id: confirmForceMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (root.pendingForceLockDevice
                                        && vaultController.selectedDevice !== root.pendingForceLockDevice)
                                    vaultController.setSelectedDevice(root.pendingForceLockDevice)
                                forceLockOverlay.closeOverlay()
                                vaultController.forceLockVault()
                            }
                        }
                    }
                }
            }
        }
    }
}
