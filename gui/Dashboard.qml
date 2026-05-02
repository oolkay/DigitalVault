import QtQuick
import QtQuick.Controls
import DigitalVault
import QtQuick.Layouts
import QtQuick.Dialogs

Item {
    id: dashboardPage

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        Column {
            width: parent.width - 24
            spacing: 24
            leftPadding: 12
            rightPadding: 12
            topPadding: 12
            bottomPadding: 24

            // Title row
            RowLayout {
                width: parent.width
                Text {
                    text: "Dashboard"
                    color: Theme.textPrimary
                    font { pixelSize: 26; bold: true; family: "Inter" }
                    Behavior on color { ColorAnimation { duration: 250 } }
                }
                Item { Layout.fillWidth: true }
            }

            // ── Top stat cards ────────────────────────────────────
            Row {
                width: parent.width
                spacing: 16

                Repeater {
                    model: [
                        { label: "Vault Status",   value: vaultController.isLocked ? "Locked" : "Mounted",  icon: "🔒", accent: vaultController.isLocked ? Theme.danger : Theme.success },
                        { label: "Fingerprints",   value: vaultController.fingerprintCount + " registered", icon: "☝",  accent: Theme.accent },
                        { label: "Encryption",     value: "AES-256-XTS",  icon: "🔑", accent: Theme.warning },
                        { label: "HAL Status",     value: "Simulator",    icon: "⚡", accent: Theme.success }
                    ]
                    delegate: Rectangle {
                        width: (parent.width - 48) / 4
                        height: 90
                        radius: Theme.radius
                        color: Theme.bgCard
                        border.color: Theme.border
                        border.width: 1
                        Behavior on color { ColorAnimation { duration: 250 } }
                        Behavior on border.color { ColorAnimation { duration: 250 } }

                        // top accent line
                        Rectangle {
                            anchors { top: parent.top; left: parent.left; right: parent.right }
                            height: 3
                            radius: parent.radius
                            color: modelData.accent
                        }

                        Column {
                            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 16; topMargin: 18 }
                            spacing: 6
                            Row {
                                spacing: 8
                                Text { text: modelData.icon; font.pixelSize: 16 }
                                Text {
                                    text: modelData.label
                                    color: Theme.textSecondary
                                    font { pixelSize: 11; bold: true; family: "Inter"; letterSpacing: 0.5 }
                                    Behavior on color { ColorAnimation { duration: 250 } }
                                }
                            }
                            Text {
                                text: modelData.value
                                color: modelData.accent
                                font { pixelSize: 15; bold: true; family: "Inter" }
                            }
                        }
                    }
                }
            }

            // ── Middle row: vault control + devices ───────────────
            Row {
                width: parent.width
                height: 260
                spacing: 20

                // Vault control panel
                Rectangle {
                    width: parent.width * 0.42
                    height: parent.height
                    radius: Theme.radius
                    color: Theme.bgCard
                    border.color: Theme.border
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: 250 } }
                    Behavior on border.color { ColorAnimation { duration: 250 } }

                    Column {
                        anchors.centerIn: parent
                        spacing: 20

                        // Fingerprint ring
                        Item {
                            width: 110; height: 110
                            anchors.horizontalCenter: parent.horizontalCenter

                            // Outer glow ring
                            Rectangle {
                                anchors.centerIn: parent
                                width: 110; height: 110
                                radius: 55
                                color: "transparent"
                                border.color: vaultController.isLocked ? Theme.danger : Theme.success
                                border.width: 2
                                opacity: 0.4
                            }
                            // Inner circle
                            Rectangle {
                                anchors.centerIn: parent
                                width: 84; height: 84
                                radius: 42
                                color: vaultController.isLocked ? Theme.dangerBg : Theme.successBg
                                Behavior on color { ColorAnimation { duration: 400 } }

                                Text {
                                    anchors.centerIn: parent
                                    text: "☝"
                                    font.pixelSize: 36
                                    color: vaultController.isLocked ? Theme.danger : Theme.success
                                    Behavior on color { ColorAnimation { duration: 400 } }
                                }
                            }
                            // Pulse animation
                            SequentialAnimation on scale {
                                running: !vaultController.isLocked
                                loops: Animation.Infinite
                                NumberAnimation { to: 1.05; duration: 800; easing.type: Easing.InOutSine }
                                NumberAnimation { to: 1.00; duration: 800; easing.type: Easing.InOutSine }
                            }
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: vaultController.vaultStatusText
                            color: Theme.textPrimary
                            font { pixelSize: 17; bold: true; family: "Inter" }
                            Behavior on color { ColorAnimation { duration: 250 } }
                        }

                        // Action button
                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: 160; height: 42
                            radius: 21
                            color: vaultController.isLocked ? Theme.success : Theme.danger
                            Behavior on color { ColorAnimation { duration: 300 } }

                            scale: btnMouse.containsMouse ? 1.04 : 1.0
                            Behavior on scale { NumberAnimation { duration: 150 } }

                            Text {
                                anchors.centerIn: parent
                                text: vaultController.isLocked ? "🔓  Unlock Vault" : "🔒  Lock Vault"
                                color: "white"
                                font { pixelSize: 14; bold: true; family: "Inter" }
                            }
                            MouseArea {
                                id: btnMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: vaultController.toggleVault()
                            }
                        }
                    }
                }

                // Connected devices list
                Rectangle {
                    width: parent.width - parent.children[0].width - 20
                    height: parent.height
                    radius: Theme.radius
                    color: Theme.bgCard
                    border.color: Theme.border
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: 250 } }
                    Behavior on border.color { ColorAnimation { duration: 250 } }

                    Column {
                        anchors { fill: parent; margins: 20 }
                        spacing: 14

                        RowLayout {
                            width: parent.width
                            Text {
                                text: "Connected Devices"
                                color: Theme.textPrimary
                                font { pixelSize: 15; bold: true; family: "Inter" }
                                Behavior on color { ColorAnimation { duration: 250 } }
                            }
                            Item { Layout.fillWidth: true }
                            Button {
                                text: "+ Create New"
                                flat: true
                                font { pixelSize: 11; bold: true }
                                palette.buttonText: Theme.accent
                                onClicked: creationDialog.open()
                            }
                        }

                        Repeater {
                            model: vaultController.getStorageDevices()
                            delegate: Rectangle {
                                width: parent.width
                                height: 62
                                radius: 10
                                color: (vaultController.isLocked && vaultController.selectedDevice === modelData.path) ? Theme.accentGlow : Theme.bgOverlay
                                border.color: vaultController.selectedDevice === modelData.path ? Theme.accent : Theme.border
                                border.width: vaultController.selectedDevice === modelData.path ? 2 : 1
                                Behavior on color { ColorAnimation { duration: 250 } }
                                Behavior on border.color { ColorAnimation { duration: 250 } }

                                Row {
                                    anchors { fill: parent; leftMargin: 14; rightMargin: 14 }
                                    spacing: 14

                                    Rectangle {
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 38; height: 38; radius: 8
                                        color: modelData.path.startsWith("/dev/") ? Theme.warningBg : Theme.accentGlow
                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.path.startsWith("/dev/") ? "💾" : "🔐"
                                            font.pixelSize: 18
                                        }
                                    }

                                    Column {
                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: 3
                                        Text {
                                            text: modelData.name
                                            color: Theme.textPrimary
                                            font { pixelSize: 13; bold: true; family: "Inter" }
                                        }
                                        Text {
                                            text: modelData.description
                                            color: Theme.textSecondary
                                            font { pixelSize: 11; family: "Inter" }
                                            width: 150
                                            elide: Text.ElideRight
                                        }
                                    }

                                    Item { Layout.fillWidth: true }

                                    Rectangle {
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: statusLabel.width + 16; height: 24; radius: 12
                                        color: vaultController.selectedDevice === modelData.path ? Theme.successBg : "transparent"
                                        border.color: vaultController.selectedDevice === modelData.path ? Theme.success : Theme.border
                                        Text {
                                            id: statusLabel
                                            anchors.centerIn: parent
                                            text: vaultController.selectedDevice === modelData.path ? "SELECTED" : "SELECT"
                                            color: vaultController.selectedDevice === modelData.path ? Theme.success : Theme.textMuted
                                            font { pixelSize: 10; bold: true; family: "Inter" }
                                        }
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        vaultController.setSelectedDevice(modelData.path);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ── Activity feed ─────────────────────────────────────
            Rectangle {
                width: parent.width
                height: 140
                radius: Theme.radius
                color: Theme.bgCard
                border.color: Theme.border
                border.width: 1
                Behavior on color { ColorAnimation { duration: 250 } }
                Behavior on border.color { ColorAnimation { duration: 250 } }

                Column {
                    anchors { fill: parent; margins: 20 }
                    spacing: 12

                    Text {
                        text: "Recent Activity"
                        color: Theme.textPrimary
                        font { pixelSize: 15; bold: true; family: "Inter" }
                        Behavior on color { ColorAnimation { duration: 250 } }
                    }

                    Repeater {
                        model: [
                            { time: "01:47", msg: "Vault mounted successfully",       icon: "✅", col: "#2ECC88" },
                            { time: "01:45", msg: "Fingerprint #1 verified",          icon: "☝",  col: "#4E6BF5" },
                            { time: "01:40", msg: "Hardware simulator connected",     icon: "⚡", col: "#F5A623" }
                        ]
                        delegate: Row {
                            width: parent.width
                            spacing: 12
                            Text { text: modelData.icon; font.pixelSize: 14 }
                            Text {
                                text: modelData.time
                                color: Theme.textMuted
                                font { pixelSize: 12; family: "Inter" }
                                Behavior on color { ColorAnimation { duration: 250 } }
                            }
                            Text {
                                text: modelData.msg
                                color: Theme.textSecondary
                                font { pixelSize: 12; family: "Inter" }
                                Behavior on color { ColorAnimation { duration: 250 } }
                            }
                        }
                    }
                }
            }
        }
    }
    FileDialog {
        id: creationDialog
        title: "Create New Vault Container"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Vault Images (*.img)", "All Files (*)"]
        onAccepted: {
            vaultController.createNewVault(creationDialog.selectedFile)
        }
    }
}
