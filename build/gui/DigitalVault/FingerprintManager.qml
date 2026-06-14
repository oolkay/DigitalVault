import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DigitalVault

Item {
    id: fpPage

    Column {
        anchors.fill: parent
        spacing: 24

        // ── Header ───────────────────────────────────────────
        Row {
            width: parent.width
            spacing: 14

            Column {
                spacing: 4
                Text {
                    text: "Fingerprint Management"
                    color: Theme.textPrimary
                    font { pixelSize: 26; bold: true; family: Theme.fontFamily }
                    Behavior on color { ColorAnimation { duration: 250 } }
                }
                Text {
                    text: vaultController.fingerprintCount + " enrolled fingerprints"
                    color: Theme.textSecondary
                    font { pixelSize: 13; family: Theme.fontFamily }
                    Behavior on color { ColorAnimation { duration: 250 } }
                }
            }

            Item { width: 1; height: 1 }

            // Add button
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 180; height: 42
                radius: 21
                color: addMouse.containsMouse ? Theme.accentHover : Theme.accent
                Behavior on color { ColorAnimation { duration: 150 } }

                scale: addMouse.containsMouse ? 1.03 : 1.0
                Behavior on scale { NumberAnimation { duration: 150 } }

                Row {
                    anchors.centerIn: parent
                    spacing: 8
                    Text { text: "+"; color: "white"; font { pixelSize: 20; bold: true } }
                    Text { text: "Add Fingerprint"; color: "white"; font { pixelSize: 14; bold: true; family: Theme.fontFamily } }
                }
                MouseArea {
                    id: addMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: vaultController.addFingerprint()
                }
            }
        }

        // ── How-to banner ────────────────────────────────────
        Rectangle {
            width: parent.width
            height: 64
            radius: Theme.radius
            color: Theme.accentGlow
            border.color: Theme.accent
            border.width: 1
            Behavior on color { ColorAnimation { duration: 250 } }

            Row {
                anchors { verticalCenter: parent.verticalCenter; left: parent.left; leftMargin: 20 }
                spacing: 14
                Text { text: "☝"; font.pixelSize: 26 }
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Text {
                        text: "Place finger on sensor"
                        color: Theme.accent
                        font { pixelSize: 13; bold: true; family: Theme.fontFamily }
                    }
                    Text {
                        text: "Add or rename fingerprints used by your vaults."
                        color: Theme.textSecondary
                        font { pixelSize: 11; family: Theme.fontFamily }
                        Behavior on color { ColorAnimation { duration: 250 } }
                    }
                }
            }
        }

        // ── Fingerprint list ─────────────────────────────────
        ListView {
            width: parent.width
            height: parent.height - 180
            spacing: 10
            clip: true
            model: vaultController.fingerprintCount

            delegate: Rectangle {
                id: fpCard
                width: parent.width
                height: 72
                radius: Theme.radius
                color: cardHover.containsMouse ? Theme.bgHover : Theme.bgCard
                border.color: Theme.border
                border.width: 1
                Behavior on color { ColorAnimation { duration: 150 } }
                
                property bool editing: false

                RowLayout {
                    anchors { fill: parent; leftMargin: 18; rightMargin: 18 }
                    spacing: 16

                    // Index badge
                    Rectangle {
                        Layout.alignment: Qt.AlignVCenter
                        width: 42; height: 42; radius: 10
                        color: Theme.accentGlow
                        Text {
                            anchors.centerIn: parent
                            text: "☝"
                            font.pixelSize: 20
                            color: Theme.accent
                        }
                    }

                    Column {
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 3
                        
                        // Name / Edit Field
                        Item {
                            width: 250; height: 24
                            
                            Text {
                                visible: !editing
                                text: vaultController.fingerprintLabel(index)
                                color: Theme.textPrimary
                                font { pixelSize: 14; bold: true; family: Theme.fontFamily }
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            
                            TextField {
                                id: editField
                                visible: editing
                                anchors.fill: parent
                                text: vaultController.fingerprintLabel(index)
                                color: Theme.textPrimary
                                font { pixelSize: 13; family: Theme.fontFamily }
                                background: Rectangle {
                                    color: Theme.bgOverlay
                                    border.color: Theme.accent
                                    radius: 4
                                }
                                onAccepted: {
                                    vaultController.renameFingerprint(index, text)
                                    editing = false
                                }
                                Component.onCompleted: if (visible) forceActiveFocus()
                            }
                        }

                        Text {
                            text: "Registered fingerprint"
                            color: Theme.textSecondary
                            font { pixelSize: 11; family: Theme.fontFamily }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    // Edit Button
                    Rectangle {
                        Layout.alignment: Qt.AlignVCenter
                        width: 34; height: 34; radius: 17
                        color: editMouse.containsMouse ? Theme.bgOverlay : "transparent"
                        Text {
                            anchors.centerIn: parent
                            text: editing ? "OK" : "Edit"
                            font.pixelSize: 14
                        }
                        MouseArea {
                            id: editMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                if (editing) {
                                    vaultController.renameFingerprint(index, editField.text)
                                }
                                editing = !editing
                            }
                        }
                    }

                    Item { width: 1; height: 1 }

                    // Status chip
                    Rectangle {
                        Layout.alignment: Qt.AlignVCenter
                        width: chipLabel.width + 16; height: 24; radius: 12
                        color: Theme.successBg
                        Behavior on color { ColorAnimation { duration: 250 } }
                        Text {
                            id: chipLabel
                            anchors.centerIn: parent
                            text: "Active"
                            color: Theme.success
                            font { pixelSize: 11; bold: true; family: Theme.fontFamily }
                        }
                    }

                    // Remove button
                    Rectangle {
                        Layout.alignment: Qt.AlignVCenter
                        width: 90; height: 34; radius: 17
                        color: rmMouse.containsMouse ? Theme.dangerBg : "transparent"
                        border.color: rmMouse.containsMouse ? Theme.danger : Theme.border
                        border.width: 1
                        Behavior on color { ColorAnimation { duration: 150 } }

                        Text {
                            anchors.centerIn: parent
                            text: "Remove"
                            color: Theme.danger
                            font { pixelSize: 12; bold: true; family: Theme.fontFamily }
                        }
                        MouseArea {
                            id: rmMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: vaultController.removeFingerprint(index)
                        }
                    }
                }

                MouseArea { id: cardHover; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
            }

            // Empty state
            Text {
                anchors.centerIn: parent
                text: "No fingerprints enrolled yet.\nPress \"Add Fingerprint\" to register."
                horizontalAlignment: Text.AlignHCenter
                color: Theme.textMuted
                font { pixelSize: 14; family: Theme.fontFamily }
                visible: vaultController.fingerprintCount === 0
                Behavior on color { ColorAnimation { duration: 250 } }
            }
        }
    }
}
