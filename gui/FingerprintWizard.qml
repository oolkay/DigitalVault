import QtQuick
import QtQuick.Controls
import DigitalVault

Item {
    id: root
    anchors.fill: parent
    visible: vaultController.fingerprintUnlockActive
    z: 300

    ModalScrim {
        anchors.fill: parent
        active: root.visible
        blocking: true
    }

    property int state: 0

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(400, parent.width - 48)
        radius: Theme.radius
        color: Theme.bgCard
        border.color: Theme.border
        implicitHeight: col.implicitHeight + 40

        Column {
            id: col
            anchors.centerIn: parent
            width: parent.width - 40
            spacing: 16

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: {
                    var flow = vaultController.fingerprintFlow
                    if (flow === "setup_enroll") return "Parmak izi kaydı"
                    if (flow === "setup_verify") return "Kurulum doğrulaması"
                    if (root.state === 1) return "✓ Parmak izi doğrulandı"
                    if (root.state === 2) return "Doğrulama başarısız"
                    return "Parmağınızı sensöre koyun"
                }
                color: Theme.textPrimary
                font { pixelSize: 18; bold: true; family: "Inter" }
            }

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: vaultController.fingerprintUnlockMessage
                color: Theme.textSecondary
                font { pixelSize: 13; family: "Inter" }
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 12

                Button {
                    text: "İptal"
                    enabled: root.state === 0
                    onClicked: vaultController.cancelFingerprintUnlock()
                }
            }
        }
    }

    Connections {
        target: vaultController
        function onFingerprintUnlockActiveChanged() {
            if (vaultController.fingerprintUnlockActive)
                root.state = 0
        }
    }
}
