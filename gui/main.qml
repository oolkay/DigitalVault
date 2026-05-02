import QtQuick
import QtQuick.Controls
import DigitalVault

Window {
    id: root
    width: 1160
    height: 740
    minimumWidth: 940
    minimumHeight: 600
    visible: true
    title: "VaultGuard — Biometric Digital Vault"
    color: Theme.bgBase

    // ── Backend Controller ────────────────────────────────────
    VaultController {
        id: vaultController
    }

    // ── Smooth theme transition ───────────────────────────────
    Behavior on color { ColorAnimation { duration: 250 } }

    // ── Sidebar ──────────────────────────────────────────────
    Rectangle {
        id: sidebar
        width: 230
        height: parent.height
        color: Theme.bgSidebar
        layer.enabled: true

        Behavior on color { ColorAnimation { duration: 250 } }

        // Right border
        Rectangle {
            anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
            width: 1
            color: Theme.border
            Behavior on color { ColorAnimation { duration: 250 } }
        }

        Column {
            anchors { top: parent.top; left: parent.left; right: parent.right; topMargin: 28 }
            spacing: 0

            // ── Logo ─────────────────────────────────
            Item {
                width: parent.width
                height: 72
                Row {
                    anchors { verticalCenter: parent.verticalCenter; left: parent.left; leftMargin: 22 }
                    spacing: 12
                    Rectangle {
                        width: 38; height: 38
                        radius: 10
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: "#4E6BF5" }
                            GradientStop { position: 1.0; color: "#7A5AF8" }
                        }
                        Text {
                            anchors.centerIn: parent
                            text: "🔐"
                            font.pixelSize: 18
                        }
                    }
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 1
                        Text {
                            text: "VaultGuard"
                            color: Theme.textPrimary
                            font { pixelSize: 17; bold: true; family: "Inter" }
                            Behavior on color { ColorAnimation { duration: 250 } }
                        }
                        Text {
                            text: "Biometric Vault"
                            color: Theme.textMuted
                            font { pixelSize: 11; family: "Inter" }
                            Behavior on color { ColorAnimation { duration: 250 } }
                        }
                    }
                }
            }

            // ── Divider ──────────────────────────────
            Rectangle { width: parent.width; height: 1; color: Theme.border; Behavior on color { ColorAnimation { duration: 250 } } }
            Item { width: 1; height: 16 }

            // ── Nav label ────────────────────────────
            Text {
                leftPadding: 22
                bottomPadding: 6
                text: "MAIN MENU"
                color: Theme.textMuted
                font { pixelSize: 10; bold: true; letterSpacing: 1.5; family: "Inter" }
                Behavior on color { ColorAnimation { duration: 250 } }
            }

            // ── Nav Items ────────────────────────────
            Repeater {
                id: navRepeater
                property int currentIndex: 0
                model: [
                    { label: "Dashboard",    icon: "⬡", page: "Dashboard.qml" },
                    { label: "Fingerprints", icon: "☝", page: "FingerprintManager.qml" },
                    { label: "Security Log", icon: "📋", page: "SecurityLog.qml" },
                    { label: "Settings",     icon: "⚙", page: "Settings.qml" }
                ]

                delegate: Item {
                    width: parent.width
                    height: 46

                    property bool isActive: navRepeater.currentIndex === index

                    // Active indicator bar
                    Rectangle {
                        anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
                        width: 3
                        radius: 2
                        color: Theme.accent
                        visible: isActive
                    }

                    Rectangle {
                        id: navBg
                        anchors { fill: parent; leftMargin: 8; rightMargin: 8; topMargin: 2; bottomMargin: 2 }
                        radius: Theme.radiusSm
                        color: isActive ? Theme.navActive : (navMouse.containsMouse ? Theme.bgHover : "transparent")
                        Behavior on color { ColorAnimation { duration: 150 } }

                        Row {
                            anchors { verticalCenter: parent.verticalCenter; left: parent.left; leftMargin: 12 }
                            spacing: 12
                            Text {
                                text: modelData.icon
                                font.pixelSize: 16
                                color: isActive ? Theme.accent : Theme.textSecondary
                                Behavior on color { ColorAnimation { duration: 150 } }
                            }
                            Text {
                                text: modelData.label
                                color: isActive ? Theme.navActiveText : Theme.textSecondary
                                font { pixelSize: 14; bold: isActive; family: "Inter" }
                                Behavior on color { ColorAnimation { duration: 150 } }
                            }
                        }

                        MouseArea {
                            id: navMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                navRepeater.currentIndex = index
                                stackView.replace(modelData.page)
                            }
                        }
                    }
                }
            }

            // ── Spacer ───────────────────────────────
            Item { width: 1; height: 24 }
            Rectangle { width: parent.width; height: 1; color: Theme.border; Behavior on color { ColorAnimation { duration: 250 } } }
            Item { width: 1; height: 12 }

            // ── Vault status mini chip ────────────────
            Item {
                width: parent.width
                height: 54
                Rectangle {
                    anchors { fill: parent; leftMargin: 14; rightMargin: 14; topMargin: 4; bottomMargin: 4 }
                    radius: 10
                    color: vaultController.isLocked ? Theme.dangerBg : Theme.successBg
                    Behavior on color { ColorAnimation { duration: 300 } }

                    Row {
                        anchors { verticalCenter: parent.verticalCenter; left: parent.left; leftMargin: 12 }
                        spacing: 10
                        Rectangle {
                            width: 8; height: 8; radius: 4
                            anchors.verticalCenter: parent.verticalCenter
                            color: vaultController.isLocked ? Theme.danger : Theme.success
                            SequentialAnimation on opacity {
                                running: true; loops: Animation.Infinite
                                NumberAnimation { to: 0.3; duration: 900 }
                                NumberAnimation { to: 1.0; duration: 900 }
                            }
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: vaultController.isLocked ? "Vault Locked" : "Vault Open"
                            color: vaultController.isLocked ? Theme.danger : Theme.success
                            font { pixelSize: 13; bold: true; family: "Inter" }
                        }
                    }
                }
            }
        }

        // ── Theme toggle + version at bottom ──────────
        Column {
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right; bottomMargin: 20 }
            spacing: 0

            Rectangle { width: parent.width; height: 1; color: Theme.border; Behavior on color { ColorAnimation { duration: 250 } } }
            Item { width: 1; height: 14 }

            // Theme toggle
            Item {
                width: parent.width
                height: 40
                Row {
                    anchors { verticalCenter: parent.verticalCenter; left: parent.left; leftMargin: 22 }
                    spacing: 10
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: Theme.isDark ? "🌙" : "☀️"
                        font.pixelSize: 16
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: Theme.isDark ? "Dark Mode" : "Light Mode"
                        color: Theme.textSecondary
                        font { pixelSize: 13; family: "Inter" }
                        Behavior on color { ColorAnimation { duration: 250 } }
                    }
                    // Toggle switch
                    Rectangle {
                        id: toggleSwitch
                        anchors.verticalCenter: parent.verticalCenter
                        width: 42; height: 24
                        radius: 12
                        color: Theme.isDark ? Theme.accent : Theme.border
                        Behavior on color { ColorAnimation { duration: 200 } }

                        Rectangle {
                            id: toggleKnob
                            width: 18; height: 18
                            radius: 9
                            color: "white"
                            anchors.verticalCenter: parent.verticalCenter
                            x: Theme.isDark ? 20 : 4
                            Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.OutQuad } }
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: Theme.toggle()
                        }
                    }
                }
            }

            Item {
                width: parent.width; height: 28
                Text {
                    anchors { left: parent.left; leftMargin: 22; verticalCenter: parent.verticalCenter }
                    text: "v1.0.0 — CSE 496"
                    color: Theme.textMuted
                    font { pixelSize: 10; family: "Inter" }
                    Behavior on color { ColorAnimation { duration: 250 } }
                }
            }
        }
    }

    // ── Main Content ─────────────────────────────────────────
    Rectangle {
        id: mainContent
        anchors { left: sidebar.right; right: parent.right; top: parent.top; bottom: parent.bottom }
        color: Theme.bgBase
        Behavior on color { ColorAnimation { duration: 250 } }

        StackView {
            id: stackView
            anchors { fill: parent; margins: 32 }
            initialItem: "Dashboard.qml"

            replaceEnter: Transition {
                ParallelAnimation {
                    NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 280; easing.type: Easing.OutCubic }
                    NumberAnimation { property: "y"; from: 12; to: 0; duration: 280; easing.type: Easing.OutCubic }
                }
            }
            replaceExit: Transition {
                NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 150 }
            }
        }
    }

    // ── Toast Notification ───────────────────────────────────
    Rectangle {
        id: toastBar
        anchors { bottom: parent.bottom; horizontalCenter: mainContent.horizontalCenter; bottomMargin: 28 }
        width: toastText.width + 56
        height: 48
        radius: 24
        color: toastIsError ? Theme.dangerBg : Theme.bgSurface
        border.color: toastIsError ? Theme.danger : Theme.border
        border.width: 1
        opacity: 0
        visible: opacity > 0

        property bool toastIsError: false

        Behavior on opacity { NumberAnimation { duration: 220 } }
        Behavior on color { ColorAnimation { duration: 220 } }

        Row {
            anchors.centerIn: parent
            spacing: 10
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: toastBar.toastIsError ? "⚠" : "✓"
                color: toastBar.toastIsError ? Theme.danger : Theme.success
                font.pixelSize: 16
            }
            Text {
                id: toastText
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.textPrimary
                font { pixelSize: 14; family: "Inter" }
                Behavior on color { ColorAnimation { duration: 250 } }
            }
        }

        Timer {
            id: toastTimer
            interval: 3200
            onTriggered: toastBar.opacity = 0
        }

        Connections {
            target: vaultController
            function onNotificationReceived(message, isError) {
                toastText.text = message
                toastBar.toastIsError = isError
                toastBar.opacity = 1
                toastTimer.restart()
            }
        }
    }
}
