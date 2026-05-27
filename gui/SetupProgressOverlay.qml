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
                Text { text: "🔐"; font.pixelSize: 22; anchors.verticalCenter: parent.verticalCenter }
                Text {
                    text: "Formatting Vault"
                    color: Theme.textPrimary
                    font { pixelSize: 18; bold: true; family: "Inter" }
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Text {
                width: parent.width
                text: vaultController.setupStatusText
                color: Theme.textSecondary
                font { pixelSize: 11; family: "Inter" }
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            Rectangle {
                id: progressTrack
                width: parent.width
                height: 6
                radius: 3
                color: Theme.bgOverlay
                clip: true

                Rectangle {
                    height: parent.height
                    radius: parent.radius
                    color: Theme.accent
                    SequentialAnimation on width {
                        running: vaultController.isSetupRunning
                        loops: Animation.Infinite
                        NumberAnimation {
                            from: 0
                            to: progressTrack.width
                            duration: 1800
                            easing.type: Easing.InOutQuart
                        }
                        NumberAnimation {
                            from: progressTrack.width
                            to: 0
                            duration: 1800
                            easing.type: Easing.InOutQuart
                        }
                    }
                }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Do not close the application."
                color: Theme.textMuted
                font { pixelSize: 10; family: "Inter" }
            }
        }
    }
}
