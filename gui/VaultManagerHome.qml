import QtQuick
import QtQuick.Controls
import DigitalVault

Item {
    id: root
    width: parent ? parent.width : 0
    height: parent ? parent.height : 0

    signal addVaultRequested()
    signal vaultOpenRequested(string path)

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        Column {
            width: parent.width - 24
            spacing: 20
            leftPadding: 12
            rightPadding: 12
            topPadding: 12
            bottomPadding: 24

            Column {
                width: parent.width
                spacing: 6

                Text {
                    text: "Vault Manager"
                    color: Theme.textPrimary
                    font { pixelSize: 26; bold: true; family: Theme.fontFamily }
                }

                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: "Your encrypted vaults. Daily unlock lives on the Dashboard."
                    color: Theme.textSecondary
                    font { pixelSize: 13; family: Theme.fontFamily }
                }

                Text {
                    text: "Unlock on Dashboard →"
                    color: Theme.accent
                    font { pixelSize: 11; family: Theme.fontFamily }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: vaultController.navigateToPage("Dashboard.qml")
                    }
                }
            }

            Rectangle {
                width: addBtnLbl.implicitWidth + 32
                height: 40
                radius: 20
                color: addBtnMouse.containsMouse ? Theme.accentHover : Theme.accent

                Text {
                    id: addBtnLbl
                    anchors.centerIn: parent
                    text: "+ New vault"
                    color: "white"
                    font { pixelSize: 13; bold: true; family: Theme.fontFamily }
                }

                MouseArea {
                    id: addBtnMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.addVaultRequested()
                }
            }

            Column {
                width: parent.width
                spacing: 10

                Text {
                    text: "Your vaults"
                    color: Theme.textPrimary
                    font { pixelSize: 16; bold: true; family: Theme.fontFamily }
                }

                Repeater {
                    model: vaultController.managedDevices
                    delegate: ManagedDeviceRow {
                        width: parent.width
                        deviceData: modelData
                        onRowClicked: root.vaultOpenRequested(modelData.path)
                    }
                }

                Rectangle {
                    width: parent.width
                    visible: vaultController.managedDevices.length === 0
                    radius: Theme.radius
                    color: Theme.bgCard
                    border.color: Theme.border
                    border.width: 1
                    height: emptyCol.implicitHeight + 32

                    Column {
                        id: emptyCol
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 12

                        Text {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                            text: "No vaults yet"
                            color: Theme.textPrimary
                            font { pixelSize: 15; bold: true; family: Theme.fontFamily }
                        }

                        Text {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                            text: "Add a vault from a connected disk or create a virtual container."
                            color: Theme.textMuted
                            font { pixelSize: 12; family: Theme.fontFamily }
                        }

                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: emptyCtaLbl.implicitWidth + 28
                            height: 36
                            radius: 18
                            color: emptyCtaMouse.containsMouse ? Theme.accentHover : Theme.accent

                            Text {
                                id: emptyCtaLbl
                                anchors.centerIn: parent
                                text: "+ New vault"
                                color: "white"
                                font { pixelSize: 12; bold: true; family: Theme.fontFamily }
                            }

                            MouseArea {
                                id: emptyCtaMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.addVaultRequested()
                            }
                        }
                    }
                }
            }
        }
    }
}
