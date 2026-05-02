import QtQuick
import QtQuick.Controls
import DigitalVault
import QtQuick.Layouts

Item {
    id: logPage



    Column {
        anchors.fill: parent
        spacing: 24

        // ── Header ───────────────────────────────────────────
        RowLayout {
            width: parent.width
            spacing: 14

            Column {
                spacing: 4
                Text {
                    text: "Security Log"
                    color: Theme.textPrimary
                    font { pixelSize: 26; bold: true; family: "Inter" }
                }
                Text {
                    text: vaultController.securityLogs.length + " events recorded this session"
                    color: Theme.textSecondary
                    font { pixelSize: 13; family: "Inter" }
                }
            }

            Item { Layout.fillWidth: true }

            // Clear button (Just for UI here, could be implemented in C++)
            Rectangle {
                Layout.alignment: Qt.AlignVCenter
                width: 110; height: 38; radius: 19
                color: clrMouse.containsMouse ? Theme.dangerBg : "transparent"
                border.color: Theme.border; border.width: 1
                Text {
                    anchors.centerIn: parent
                    text: "🗑  Clear"
                    color: Theme.danger
                    font { pixelSize: 13; bold: true; family: "Inter" }
                }
                MouseArea {
                    id: clrMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        // TODO: Implement clearLogs in C++
                    }
                }
            }
        }
        
        // ... Filter chips can be added back if needed, but keeping it simple for now

        // ── Log table ─────────────────────────────────────────
        Rectangle {
            width: parent.width
            height: parent.height - 120
            radius: Theme.radius
            color: Theme.bgCard
            border.color: Theme.border
            border.width: 1
            clip: true

            ListView {
                anchors { fill: parent; margins: 2; topMargin: 40 }
                spacing: 2
                model: vaultController.securityLogs

                delegate: Rectangle {
                    width: parent.width
                    height: 42
                    radius: 8
                    color: "transparent"

                    Row {
                        anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
                        spacing: 16
                        Text {
                            width: 70
                            text: modelData.ts
                            color: Theme.textMuted
                            font { pixelSize: 12; family: "Inter" }
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Rectangle {
                            width: 70; height: 22; radius: 11
                            color: modelData.level === "SUCCESS" ? Theme.successBg : (modelData.level === "ERROR" ? Theme.dangerBg : Theme.bgOverlay)
                            Text {
                                anchors.centerIn: parent
                                text: modelData.level
                                color: modelData.level === "SUCCESS" ? Theme.success : (modelData.level === "ERROR" ? Theme.danger : Theme.textSecondary)
                                font { pixelSize: 9; bold: true }
                            }
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: modelData.msg
                            color: Theme.textPrimary
                            font { pixelSize: 13; family: "Inter" }
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }
            
            // Header overlay
            Rectangle {
                width: parent.width; height: 35
                color: Theme.bgOverlay
                Row {
                    anchors { fill: parent; leftMargin: 16 }
                    spacing: 16
                    Text { width: 70; text: "TIME"; color: Theme.textMuted; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
                    Text { width: 70; text: "LEVEL"; color: Theme.textMuted; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
                    Text { text: "EVENT"; color: Theme.textMuted; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
                }
            }
        }
    }
}
