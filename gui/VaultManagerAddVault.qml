import QtQuick
import QtQuick.Controls
import DigitalVault

Item {
    id: root
    width: parent ? parent.width : 0
    height: parent ? parent.height : 0

    signal backRequested()
    signal setupRequested(string path)
    signal createLoopRequested()

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        Column {
            width: parent.width - 24
            spacing: 24
            leftPadding: 12
            rightPadding: 12
            topPadding: 4
            bottomPadding: 24

            VaultManagerNavBar {
                width: parent.width
                title: "New vault"
                subtitle: "Set up encryption on a disk or create a virtual container"
                onBackRequested: root.backRequested()
            }

            Column {
                width: parent.width
                spacing: 10

                Text {
                    text: "Connected storage"
                    color: Theme.textPrimary
                    font { pixelSize: 16; bold: true; family: "Inter" }
                }

                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: "Physical disks and partitions without a vault yet."
                    color: Theme.textSecondary
                    font { pixelSize: 12; family: "Inter" }
                }

                Repeater {
                    model: vaultController.discoveredStorageDevices
                    delegate: DiscoveredStorageRow {
                        width: parent.width
                        deviceData: modelData
                        onSetupRequested: root.setupRequested(modelData.path)
                    }
                }

                Text {
                    width: parent.width
                    visible: vaultController.discoveredStorageDevices.length === 0
                    text: "No additional disks detected"
                    color: Theme.textMuted
                    font { pixelSize: 12; family: "Inter" }
                }
            }

            Rectangle {
                width: parent.width
                radius: Theme.radius
                color: Theme.bgCard
                border.color: Theme.border
                border.width: 1
                implicitHeight: virtualCol.implicitHeight + 32

                Column {
                    id: virtualCol
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 10

                    Text {
                        text: "Virtual containers"
                        color: Theme.textPrimary
                        font { pixelSize: 14; bold: true; family: "Inter" }
                    }

                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: "Sparse image + loop device. After creation, encrypt via Set up encryption on the managed entry."
                        color: Theme.textSecondary
                        font { pixelSize: 11; family: "Inter" }
                    }

                    Rectangle {
                        width: loopBtnLbl.implicitWidth + 28
                        height: 36
                        radius: 18
                        color: loopBtnMouse.containsMouse ? Theme.accentHover : Theme.accent

                        Text {
                            id: loopBtnLbl
                            anchors.centerIn: parent
                            text: "+ Create loop device"
                            color: "white"
                            font { pixelSize: 12; bold: true; family: "Inter" }
                        }

                        MouseArea {
                            id: loopBtnMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.createLoopRequested()
                        }
                    }
                }
            }
        }
    }
}
