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
                    font { pixelSize: 22; bold: true; family: "Inter" }
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                }

                Text {
                    text: {
                        var sz = detail.sizeLabel || ""
                        return sz ? (sz + " encrypted partition") : "Encrypted vault"
                    }
                    color: Theme.textSecondary
                    font { pixelSize: 13; family: "Inter" }
                }

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: detail.encryptionLabel
                           ? ("Encryption: " + detail.encryptionLabel)
                           : "Encryption: AES-256-XTS (LUKS2)"
                    color: Theme.textSecondary
                    font { pixelSize: 12; family: "Inter" }
                }

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: "This vault is encrypted with AES-256-XTS in LUKS2 mode."
                    color: Theme.textMuted
                    font { pixelSize: 11; family: "Inter" }
                }

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: {
                        var p = detail.devicePath || detail.path || devicePath || ""
                        return p ? ("Device: " + p) : ""
                    }
                    color: Theme.textMuted
                    font { pixelSize: 11; family: "monospace" }
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
                        font { pixelSize: 12; bold: true; family: "Inter" }
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

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "Authorized access"
                color: Theme.textMuted
                font { pixelSize: 10; bold: true; family: "Inter"; letterSpacing: 1 }
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: "People who can unlock this vault."
                color: Theme.textSecondary
                font { pixelSize: 11; family: "Inter" }
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
                    font { pixelSize: 14; family: "Inter" }
                    Layout.fillWidth: true
                }

                Text {
                    text: accessListExpanded ? "Hide list" : "Show list"
                    color: Theme.accent
                    font { pixelSize: 12; underline: true; family: "Inter" }
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
                                    text: modelData.isOwner ? "👑" : "🔑"
                                    font.pixelSize: 14
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    width: Math.max(0, parent.width - 26)
                                    text: modelData.label || "Unnamed fingerprint"
                                    color: Theme.textPrimary
                                    font { pixelSize: 12; family: "Inter" }
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
                font { pixelSize: 11; family: "Inter" }
            }

        }

        Item { Layout.fillHeight: true }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                radius: 22
                color: {
                    if (vaultController.isToggling
                        && (devicePath === vaultController.unlockedDevice
                            || devicePath === vaultController.selectedDevice))
                        return "#444"
                    return detail.isUnlocked ? Theme.danger : Theme.success
                }
                opacity: devicePath ? 1 : 0.5

                Text {
                    anchors.centerIn: parent
                    text: {
                        if (vaultController.isToggling
                            && (devicePath === vaultController.unlockedDevice
                                || devicePath === vaultController.selectedDevice))
                            return detail.isUnlocked ? "⏳ Locking…" : "⏳ Decrypting…"
                        return detail.isUnlocked ? "🔒  Lock Vault" : "🔓  Unlock Vault"
                    }
                    color: "white"
                    font { pixelSize: 13; bold: true; family: "Inter" }
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
                    radius: 18
                    color: filesMouse.containsMouse ? Theme.accentHover : Theme.accent
                    Text {
                        anchors.centerIn: parent
                        text: "📁  Open Files"
                        color: "white"
                        font { pixelSize: 12; bold: true; family: "Inter" }
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
                    radius: 18
                    color: termMouse.containsMouse ? Theme.bgHover : Theme.bgOverlay
                    border.color: Theme.border
                    border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text: "⌨  Terminal"
                        color: Theme.textPrimary
                        font { pixelSize: 12; bold: true; family: "Inter" }
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
}
