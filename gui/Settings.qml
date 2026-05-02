import QtQuick
import QtQuick.Controls
import DigitalVault
import QtQuick.Layouts

Item {
    id: settingsPage

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

            Text {
                text: "Settings"
                color: Theme.textPrimary
                font { pixelSize: 26; bold: true; family: "Inter" }
                Behavior on color { ColorAnimation { duration: 250 } }
            }

            // ── Settings sections ─────────────────────────────────
            Column {
                width: parent.width
                spacing: 16

                // ── Appearance ───────────────────────────────────
                SettingsSection {
                    title: "Appearance"
                    icon: "🎨"
                    width: parent.width

                    SettingsRow {
                        width: parent.width
                        label: "Theme"
                        description: "Switch between dark and light mode"
                        control: Row {
                            spacing: 10
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: Theme.isDark ? "Dark" : "Light"
                                color: Theme.textSecondary
                                font { pixelSize: 13; family: "Inter" }
                                Behavior on color { ColorAnimation { duration: 250 } }
                            }
                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 46; height: 26; radius: 13
                                color: Theme.isDark ? Theme.accent : Theme.border
                                Behavior on color { ColorAnimation { duration: 200 } }
                                Rectangle {
                                    width: 20; height: 20; radius: 10; color: "white"
                                    anchors.verticalCenter: parent.verticalCenter
                                    x: Theme.isDark ? 22 : 4
                                    Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.OutQuad } }
                                }
                                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Theme.toggle() }
                            }
                        }
                    }
                }

                // ── Security ─────────────────────────────────────
                SettingsSection {
                    title: "Security"
                    icon: "🔐"
                    width: parent.width

                    SettingsRow {
                        width: parent.width
                        label: "Auto-lock timeout"
                        description: "Lock vault after inactivity"
                        control: Rectangle {
                            width: 140; height: 34; radius: 8
                            color: Theme.bgInput
                            border.color: Theme.border
                            border.width: 1
                            Behavior on color { ColorAnimation { duration: 250 } }

                            Row {
                                anchors { fill: parent; leftMargin: 12; rightMargin: 8 }
                                spacing: 6
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: "5 minutes"
                                    color: Theme.textPrimary
                                    font { pixelSize: 13; family: "Inter" }
                                    Behavior on color { ColorAnimation { duration: 250 } }
                                }
                                Item { width: 1; height: 1; Layout.fillWidth: true }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: "▾"
                                    color: Theme.textMuted
                                    font.pixelSize: 12
                                }
                            }
                        }
                    }

                    SettingsRow {
                        width: parent.width
                        label: "Hardware disconnect lockdown"
                        description: "Automatically unmount vault when STM32 is removed"
                        control: ToggleSwitch { checked: true }
                    }

                    SettingsRow {
                        width: parent.width
                        label: "Require fingerprint on mount"
                        description: "Always verify biometrics before accessing vault"
                        control: ToggleSwitch { checked: true }
                    }
                }

                // ── Storage ──────────────────────────────────────
                SettingsSection {
                    title: "Storage"
                    icon: "💾"
                    width: parent.width

                    SettingsRow {
                        width: parent.width
                        label: "Vault image path"
                        description: "Location of the encrypted vault.img file"
                        control: Rectangle {
                            width: 220; height: 34; radius: 8
                            color: Theme.bgInput
                            border.color: Theme.border
                            border.width: 1
                            Behavior on color { ColorAnimation { duration: 250 } }
                            Text {
                                anchors { verticalCenter: parent.verticalCenter; left: parent.left; leftMargin: 12 }
                                text: "./fuse_vault/vault.img"
                                color: Theme.textSecondary
                                font { pixelSize: 12; family: "Courier New" }
                                Behavior on color { ColorAnimation { duration: 250 } }
                            }
                        }
                    }

                    SettingsRow {
                        width: parent.width
                        label: "Mount point"
                        description: "Where vault is exposed in your filesystem"
                        control: Rectangle {
                            width: 220; height: 34; radius: 8
                            color: Theme.bgInput
                            border.color: Theme.border
                            border.width: 1
                            Behavior on color { ColorAnimation { duration: 250 } }
                            Text {
                                anchors { verticalCenter: parent.verticalCenter; left: parent.left; leftMargin: 12 }
                                text: "~/VaultMount"
                                color: Theme.textSecondary
                                font { pixelSize: 12; family: "Courier New" }
                                Behavior on color { ColorAnimation { duration: 250 } }
                            }
                        }
                    }
                }

                // ── About ────────────────────────────────────────
                SettingsSection {
                    title: "About"
                    icon: "ℹ"
                    width: parent.width

                    SettingsRow {
                        width: parent.width
                        label: "Version"
                        description: "VaultGuard release version"
                        control: Text {
                            text: "v1.0.0"
                            color: Theme.accent
                            font { pixelSize: 13; bold: true; family: "Inter" }
                        }
                    }
                    SettingsRow {
                        width: parent.width
                        label: "Project"
                        description: "Gebze Technical University — CSE 496"
                        control: Text {
                            text: "Ömer Faruk Olkay"
                            color: Theme.textSecondary
                            font { pixelSize: 13; family: "Inter" }
                            Behavior on color { ColorAnimation { duration: 250 } }
                        }
                    }
                }
            }
        }
    }

    // ── Helper components ─────────────────────────────────────
    component SettingsSection: Rectangle {
        property string title: ""
        property string icon: ""
        default property alias content: innerCol.children
        height: innerCol.height + 58
        radius: Theme.radius
        color: Theme.bgCard
        border.color: Theme.border
        border.width: 1
        Behavior on color { ColorAnimation { duration: 250 } }
        Behavior on border.color { ColorAnimation { duration: 250 } }

        Column {
            id: innerCol
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 20 }
            spacing: 0

            // Section header
            Row {
                height: 38
                spacing: 10
                Text { anchors.verticalCenter: parent.verticalCenter; text: icon; font.pixelSize: 16 }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: title
                    color: Theme.textPrimary
                    font { pixelSize: 14; bold: true; family: "Inter" }
                    Behavior on color { ColorAnimation { duration: 250 } }
                }
            }
            Rectangle { width: parent.width; height: 1; color: Theme.border; Behavior on color { ColorAnimation { duration: 250 } } }
        }
    }

    component SettingsRow: Item {
        property string label: ""
        property string description: ""
        property alias control: controlSlot.children
        height: 62

        Row {
            anchors { fill: parent; topMargin: 8; bottomMargin: 8 }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - controlSlot.width - 10
                spacing: 3
                Text {
                    text: label
                    color: Theme.textPrimary
                    font { pixelSize: 13; bold: true; family: "Inter" }
                    Behavior on color { ColorAnimation { duration: 250 } }
                }
                Text {
                    text: description
                    color: Theme.textSecondary
                    font { pixelSize: 11; family: "Inter" }
                    Behavior on color { ColorAnimation { duration: 250 } }
                }
            }
            Item { id: controlSlot; anchors.verticalCenter: parent.verticalCenter }
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width; height: 1
            color: Theme.border
            opacity: 0.5
            Behavior on color { ColorAnimation { duration: 250 } }
        }
    }

    component ToggleSwitch: Rectangle {
        property bool checked: false
        width: 46; height: 26; radius: 13
        color: checked ? Theme.accent : Theme.border
        Behavior on color { ColorAnimation { duration: 200 } }
        Rectangle {
            width: 20; height: 20; radius: 10; color: "white"
            anchors.verticalCenter: parent.verticalCenter
            x: parent.checked ? 22 : 4
            Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.OutQuad } }
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.checked = !parent.checked
        }
    }
}
