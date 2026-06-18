import QtQuick
import DigitalVault

Item {
    id: root

    property Item blurSource: null
  property bool backdropEnabled: true

    anchors.fill: parent
    visible: vaultController.isSetupRunning
    z: 250

    ModalScrim {
        anchors.fill: parent
        sourceItem: root.blurSource
        active: root.visible && root.backdropEnabled
        blocking: true
        dismissible: false
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.visible
    }

    Rectangle {
        anchors.centerIn: parent
        width: 360
        height: 200
        radius: Theme.radius
        color: Theme.bgSurface
        border.color: Theme.border
        border.width: 1
        z: 1

        Column {
            anchors.centerIn: parent
            spacing: 20
            width: parent.width - 48

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 10
                Rectangle {
                    width: 28
                    height: 28
                    radius: Theme.radiusSm
                    color: Theme.accentGlow
                    border.color: Theme.borderFocus
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        anchors.centerIn: parent
                        text: "V"
                        color: Theme.accent
                        font { pixelSize: 12; bold: true; family: Theme.fontFamily }
                    }
                }
                Text {
                    text: "Formatting Vault"
                    color: Theme.textPrimary
                    font { pixelSize: 18; bold: true; family: Theme.fontFamily }
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Text {
                width: parent.width
                text: vaultController.setupStatusText
                color: Theme.textSecondary
                font { pixelSize: 11; family: Theme.fontFamily }
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            Item {
                id: busySpinner
                width: 42
                height: 42
                anchors.horizontalCenter: parent.horizontalCenter

                Rectangle {
                    anchors.fill: parent
                    radius: width / 2
                    color: "transparent"
                    border.color: Theme.border
                    border.width: 2
                }

                Rectangle {
                    width: 9
                    height: 9
                    radius: width / 2
                    color: Theme.accent
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: -1
                }

                RotationAnimation on rotation {
                    running: vaultController.isSetupRunning
                    from: 0
                    to: 360
                    duration: 1100
                    loops: Animation.Infinite
                    easing.type: Easing.Linear
                }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Do not close the application."
                color: Theme.textMuted
                font { pixelSize: 10; family: Theme.fontFamily }
            }
        }
    }
}
