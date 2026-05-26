import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DigitalVault

Item {
    id: dashboardPage

    Component.onCompleted: {
        if (vaultController.selectedDevice === ""
                && vaultController.registeredVaults.length > 0) {
            vaultController.setSelectedDevice(
                vaultController.registeredVaults[0].path)
        }
    }

    SetupProgressOverlay { anchors.fill: parent }

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

            Column {
                width: parent.width
                spacing: 6
                Text {
                    text: "Vaults"
                    color: Theme.textPrimary
                    font { pixelSize: 26; bold: true; family: "Inter" }
                }
                Text {
                    text: "Select a vault to unlock or manage access."
                    color: Theme.textSecondary
                    font { pixelSize: 13; family: "Inter" }
                    width: parent.width
                    wrapMode: Text.Wrap
                }
            }

            RowLayout {
                width: parent.width
                spacing: 20

                Item {
                    Layout.preferredWidth: parent.width * 0.38
                    Layout.maximumWidth: parent.width * 0.38
                    Layout.alignment: Qt.AlignTop
                    implicitHeight: vaultListCol.implicitHeight

                    Column {
                        id: vaultListCol
                        width: parent.width
                        spacing: 12

                        Repeater {
                            model: vaultController.registeredVaults
                            delegate: VaultListItem {
                                width: vaultListCol.width
                                vaultData: modelData
                                onClicked: vaultController.setSelectedDevice(modelData.path)
                            }
                        }

                        Rectangle {
                            width: parent.width
                            height: emptyState.implicitHeight + 48
                            radius: Theme.radius
                            color: Theme.bgCard
                            border.color: Theme.border
                            border.width: 1
                            visible: vaultController.registeredVaults.length === 0

                            Column {
                                id: emptyState
                                anchors.centerIn: parent
                                width: parent.width - 48
                                spacing: 16

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "🔐"
                                    font.pixelSize: 40
                                }
                                Text {
                                    width: parent.width
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.Wrap
                                    text: "No vaults on this computer yet"
                                    color: Theme.textPrimary
                                    font { pixelSize: 16; bold: true; family: "Inter" }
                                }
                                Text {
                                    width: parent.width
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.Wrap
                                    text: "Create a container and set up encryption on the Create Vault page."
                                    color: Theme.textSecondary
                                    font { pixelSize: 12; family: "Inter" }
                                }
                                Rectangle {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: createFirstBtn.implicitWidth + 32
                                    height: 40
                                    radius: 20
                                    color: ctaMouse.containsMouse ? Theme.accentHover : Theme.accent
                                    Text {
                                        id: createFirstBtn
                                        anchors.centerIn: parent
                                        text: "Create your first vault"
                                        color: "white"
                                        font { pixelSize: 12; bold: true; family: "Inter" }
                                    }
                                    MouseArea {
                                        id: ctaMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: vaultController.navigateToPage("CreateVault.qml")
                                    }
                                }
                            }
                        }
                    }
                }

                VaultDetailPanel {
                    id: detailPanel
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(420, implicitHeight)
                    Layout.alignment: Qt.AlignTop
                    devicePath: vaultController.selectedDevice
                    visible: vaultController.isSelectedDeviceVault
                        && vaultController.registeredVaults.length > 0
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 420
                    Layout.alignment: Qt.AlignTop
                    radius: Theme.radius
                    color: Theme.bgCard
                    border.color: Theme.border
                    border.width: 1
                    visible: !detailPanel.visible
                        && vaultController.registeredVaults.length > 0

                    Text {
                        anchors.centerIn: parent
                        width: parent.width - 48
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        text: "Select a vault from the list to see details and unlock."
                        color: Theme.textMuted
                        font { pixelSize: 14; family: "Inter" }
                    }
                }
            }
        }
    }
}
