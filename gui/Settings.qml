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
                    icon: "A"
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
                    icon: "S"
                    width: parent.width

                    SettingsRow {
                        width: parent.width
                        label: "Auto-lock timeout"
                        description: "Lock vault after inactivity"
                        control: ComboBox {
                            id: timeoutCombo
                            width: 140
                            model: ListModel {
                                ListElement { label: "Never"; value: 0 }
                                ListElement { label: "1 minute"; value: 1 }
                                ListElement { label: "2 minutes"; value: 2 }
                                ListElement { label: "5 minutes"; value: 5 }
                                ListElement { label: "10 minutes"; value: 10 }
                                ListElement { label: "30 minutes"; value: 30 }
                            }
                            textRole: "label"
                            
                            currentIndex: {
                                for (var i = 0; i < model.count; i++) {
                                    if (model.get(i).value === vaultController.autoLockTimeout) return i;
                                }
                                return 3; // Default 5 min
                            }
                            
                            onActivated: (index) => {
                                vaultController.autoLockTimeout = model.get(index).value
                            }

                            popup: Popup {
                                y: timeoutCombo.height + 4
                                width: timeoutCombo.width
                                padding: 1
                                contentItem: ListView {
                                    clip: true
                                    implicitHeight: contentHeight
                                    model: timeoutCombo.popup.visible ? timeoutCombo.delegateModel : null
                                    currentIndex: timeoutCombo.highlightedIndex
                                    ScrollIndicator.vertical: ScrollIndicator { }
                                }
                                background: Rectangle {
                                    color: Theme.bgCard
                                    border.color: Theme.border
                                    radius: 8
                                }
                            }

                            delegate: ItemDelegate {
                                width: timeoutCombo.width
                                height: 34
                                contentItem: Text {
                                    text: label
                                    color: highlighted ? "white" : Theme.textPrimary
                                    font { pixelSize: 13; family: "Inter" }
                                    verticalAlignment: Text.AlignVCenter
                                    leftPadding: 12
                                }
                                background: Rectangle {
                                    color: highlighted ? Theme.accent : "transparent"
                                    radius: 6
                                    anchors { fill: parent; margins: 2 }
                                }
                            }

                            contentItem: Text {
                                leftPadding: 12
                                rightPadding: timeoutCombo.indicator.width + timeoutCombo.spacing
                                text: timeoutCombo.displayText
                                font { pixelSize: 13; family: "Inter" }
                                color: Theme.textPrimary
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }

                            background: Rectangle {
                                implicitWidth: 140
                                implicitHeight: 34
                                radius: 8
                                color: Theme.bgInput
                                border.color: Theme.border
                                border.width: 1
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
                    icon: "D"
                    width: parent.width

                    SettingsRow {
                        width: parent.width
                        label: "Container storage"
                        description: "Backing images for loop devices (Create New)"
                        control: Rectangle {
                            width: Math.min(parent.width, 420); height: 34; radius: 8
                            color: Theme.bgInput
                            border.color: Theme.border
                            border.width: 1
                            Behavior on color { ColorAnimation { duration: 250 } }
                            Text {
                                anchors { verticalCenter: parent.verticalCenter; left: parent.left; leftMargin: 12; right: parent.right; rightMargin: 8 }
                                text: vaultController.defaultContainersDir()
                                color: Theme.textSecondary
                                font { pixelSize: 11; family: "Courier New" }
                                elide: Text.ElideMiddle
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
                    icon: "I"
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
        default property alias content: contentCol.children
        height: innerCol.implicitHeight + 40
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

            // İçerik satırları
            Column {
                id: contentCol
                width: parent.width
                spacing: 0
            }
        }
    }

    component SettingsRow: Item {
        property string label: ""
        property string description: ""
        property alias control: controlSlot.children
        height: 62

        Item {
            anchors { fill: parent; topMargin: 8; bottomMargin: 8 }

            Column {
                anchors.left: parent.left
                anchors.right: controlSlot.left
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 3
                Text {
                    width: parent.width
                    text: label
                    color: Theme.textPrimary
                    font { pixelSize: 13; bold: true; family: "Inter" }
                    Behavior on color { ColorAnimation { duration: 250 } }
                    wrapMode: Text.Wrap
                }
                Text {
                    width: parent.width
                    text: description
                    color: Theme.textSecondary
                    font { pixelSize: 11; family: "Inter" }
                    Behavior on color { ColorAnimation { duration: 250 } }
                    wrapMode: Text.Wrap
                }
            }
            Item { 
                id: controlSlot
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: childrenRect.width
                height: childrenRect.height
            }
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
