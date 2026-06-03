import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DigitalVault

Item {
    id: dashboardPage
    width: parent ? parent.width : 0
    height: parent ? parent.height : 0

    Component.onCompleted: {
        if (vaultController.selectedDevice === ""
                && vaultController.registeredVaults.length > 0) {
            vaultController.setSelectedDevice(
                vaultController.registeredVaults[0].path)
        }
    }

    Item {
        id: pageContent
        anchors.fill: parent

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

            Rectangle {
                width: parent.width
                height: sensorBannerLayout.implicitHeight + 20
                visible: !vaultController.sensorConnected
                radius: 8
                color: Theme.warningBg
                border.color: Theme.warning
                border.width: 1

                RowLayout {
                    id: sensorBannerLayout
                    anchors {
                        fill: parent
                        leftMargin: 14
                        rightMargin: 12
                        topMargin: 10
                        bottomMargin: 10
                    }
                    spacing: 10

                    Rectangle {
                        Layout.preferredWidth: 22
                        Layout.preferredHeight: 22
                        radius: 11
                        color: Theme.bgSurface
                        border.color: Theme.warning
                        Text {
                            anchors.centerIn: parent
                            text: "!"
                            color: Theme.warning
                            font { pixelSize: 13; bold: true; family: Theme.fontFamily }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            Layout.fillWidth: true
                            text: "Fingerprint sensor is not ready"
                            color: Theme.textPrimary
                            font { pixelSize: 12; bold: true; family: Theme.fontFamily }
                            wrapMode: Text.Wrap
                        }

                        Text {
                            Layout.fillWidth: true
                            text: vaultController.sensorStatusText
                            color: Theme.textSecondary
                            font { pixelSize: 11; family: Theme.fontFamily }
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: sensorBannerLink.implicitWidth + 22
                        Layout.preferredHeight: 32
                        radius: Theme.radiusSm
                        color: sensorBannerMouse.containsMouse ? Theme.bgHover : Theme.bgSurface
                        border.color: Theme.border

                        Text {
                            id: sensorBannerLink
                            anchors.centerIn: parent
                            text: "Open settings"
                            color: Theme.accent
                            font { pixelSize: 12; bold: true; family: Theme.fontFamily }
                        }

                        MouseArea {
                            id: sensorBannerMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: vaultController.navigateToPage("Settings.qml")
                        }
                    }
                }
            }

            Column {
                width: parent.width
                spacing: 6
                Text {
                    text: "Vaults"
                    color: Theme.textPrimary
                    font { pixelSize: 26; bold: true; family: Theme.fontFamily }
                }
                Text {
                    text: "Select a vault to unlock or manage access."
                    color: Theme.textSecondary
                    font { pixelSize: 13; family: Theme.fontFamily }
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

                                Rectangle {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: 48
                                    height: 48
                                    radius: Theme.radius
                                    color: Theme.accentGlow
                                    border.color: Theme.borderFocus
                                    Text {
                                        anchors.centerIn: parent
                                        text: "VG"
                                        color: Theme.accent
                                        font { pixelSize: 15; bold: true; family: Theme.fontFamily }
                                    }
                                }
                                Text {
                                    width: parent.width
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.Wrap
                                    text: "No vaults on this computer yet"
                                    color: Theme.textPrimary
                                    font { pixelSize: 16; bold: true; family: Theme.fontFamily }
                                }
                                Text {
                                    width: parent.width
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.Wrap
                                    text: "Set up encryption or create a container on the Vault Manager page."
                                    color: Theme.textSecondary
                                    font { pixelSize: 12; family: Theme.fontFamily }
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
                                        font { pixelSize: 12; bold: true; family: Theme.fontFamily }
                                    }
                                    MouseArea {
                                        id: ctaMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: vaultController.navigateToPage("VaultManager.qml")
                                    }
                                }
                            }
                        }
                    }
                }

                VaultDetailPanel {
                    id: detailPanel
                    Layout.fillWidth: true
                    Layout.preferredHeight: {
                        const actionExtra = detail.isUnlocked ? 46 : 0
                        if (!accessListExpanded)
                            return 420 + actionExtra
                        const count = detail.accessList ? detail.accessList.length : 0
                        return 420 + actionExtra + Math.min(count, 4) * 42
                    }
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
                        font { pixelSize: 14; family: Theme.fontFamily }
                    }
                }
            }
        }
        }
    }

    SetupProgressOverlay {
        anchors.fill: parent
        blurSource: pageContent
    }
}
