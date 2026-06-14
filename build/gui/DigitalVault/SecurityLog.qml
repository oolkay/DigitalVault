import QtQuick
import QtQuick.Controls
import DigitalVault
import QtQuick.Layouts

Item {
    id: logPage

    property int currentPage: 0
    property int pageSize: 15
    property var logData: []

    Component.onCompleted: refreshLogs()

    function refreshLogs() {
        logData = vaultController.getSecurityLogsPage(currentPage, pageSize)
    }

    Connections {
        target: vaultController
        function onLogsChanged() { refreshLogs() }
    }

    Column {
        anchors.fill: parent
        spacing: 16

        // ── Header ───────────────────────────────────────────
        RowLayout {
            width: parent.width
            spacing: 14

            Column {
                spacing: 4
                Text {
                    text: "Security History"
                    color: Theme.textPrimary
                    font { pixelSize: 26; bold: true; family: Theme.fontFamily }
                }
                Text {
                    text: "Review access attempts and system events"
                    color: Theme.textSecondary
                    font { pixelSize: 13; family: Theme.fontFamily }
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
                    text: "Clear"
                    color: Theme.danger
                    font { pixelSize: 13; bold: true; family: Theme.fontFamily }
                }
                MouseArea {
                    id: clrMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        vaultController.clearLogs()
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
                id: logList
                anchors { fill: parent; margins: 2; topMargin: 40; bottomMargin: 50 }
                spacing: 2
                model: logPage.logData
                clip: true

                delegate: Rectangle {
                    width: logList.width
                    height: 48
                    radius: 6
                    color: index % 2 === 0 ? Qt.rgba(1,1,1,0.02) : "transparent"

                    Row {
                        anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
                        spacing: 20

                        Text {
                            width: 140
                            text: modelData.time
                            color: Theme.textMuted
                            font { pixelSize: 12; family: Theme.monoFamily }
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Rectangle {
                            width: 76; height: 22; radius: 11
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
                            width: parent.width - 260
                            text: modelData.message
                            color: Theme.textPrimary
                            font { pixelSize: 13; family: Theme.fontFamily }
                            anchors.verticalCenter: parent.verticalCenter
                            elide: Text.ElideRight
                        }
                    }
                }

                // Empty state
                Text {
                    anchors.centerIn: parent
                    visible: logPage.logData.length === 0
                    text: "No logs found."
                    color: Theme.textMuted
                }
            }

            // Pagination Controls
            Rectangle {
                anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                height: 45
                color: Theme.bgOverlay
                border.color: Theme.border; border.width: 1

                Row {
                    anchors.centerIn: parent
                    spacing: 20

                    Button {
                        text: "Previous"
                        enabled: logPage.currentPage > 0
                        onClicked: {
                            logPage.currentPage--
                            logPage.refreshLogs()
                        }
                    }

                    Text {
                        text: "Page " + (logPage.currentPage + 1)
                        color: Theme.textPrimary
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Button {
                        text: "Next"
                        enabled: logPage.logData.length === logPage.pageSize
                        onClicked: {
                            logPage.currentPage++
                            logPage.refreshLogs()
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
                    spacing: 20
                    Text { width: 140; text: "TIMESTAMP"; color: Theme.textMuted; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
                    Text { width: 76; text: "LEVEL"; color: Theme.textMuted; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
                    Text { text: "EVENT MESSAGE"; color: Theme.textMuted; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
                }
            }
        }
    }
}
