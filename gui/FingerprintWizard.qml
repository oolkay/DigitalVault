import QtQuick
import QtQuick.Controls
import DigitalVault

// Global biometric overlay (single instance in main.qml). Popup layer sits above Dialogs.
Popup {
    id: root
    modal: true
    focus: true
    padding: 0
    closePolicy: Popup.NoAutoClose

    property int state: 0

    function syncOpened() {
        if (vaultController.fingerprintUnlockActive) {
            if (!opened)
                open()
        } else if (opened) {
            close()
        }
    }

    Component.onCompleted: syncOpened()

    function flowTitle(flow) {
        switch (flow) {
        case "setup_enroll": return "Fingerprint enrollment"
        case "setup_find_existing": return "Find existing fingerprint"
        case "setup_verify": return "Setup verification"
        case "grant_verify": return "Authorized verification (grant)"
        case "grant_enroll": return "Enrolling new fingerprint"
        case "revoke_verify": return "Authorized verification (revoke)"
        case "delete_verify": return "Authorized verification (delete)"
        case "unlock": return "Biometric unlock"
        default: return "Fingerprint"
        }
    }

    function headlineText() {
        const ps = vaultController.fingerprintPromptState
        if (ps === 1)
            return "Enrollment complete"
        if (ps === 2)
            return "Enrollment failed"
        if (root.state === 1)
            return "Fingerprint verified"
        if (root.state === 2)
            return "Verification failed"
        return flowTitle(vaultController.fingerprintFlow)
    }

    background: Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.65)
    }

    contentItem: Item {
        anchors.fill: parent

        Rectangle {
            id: card
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
                    text: headlineText()
                    color: Theme.textPrimary
                    font { pixelSize: 16; bold: true; family: "Inter" }
                }

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    text: vaultController.fingerprintUnlockMessage
                    color: Theme.textPrimary
                    font { pixelSize: 18; bold: true; family: "Inter" }
                }

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    visible: vaultController.fingerprintFlow === "setup_enroll"
                             && vaultController.fingerprintEnrollStep >= 2
                             && vaultController.fingerprintPromptState === 0
                    text: "Step 2 of 2"
                    color: Theme.textSecondary
                    font { pixelSize: 12; family: "Inter" }
                }

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 12

                    Button {
                        text: "Cancel"
                        onClicked: vaultController.cancelFingerprintUnlock()
                    }
                }
            }
        }
    }

    Connections {
        target: vaultController
        function onFingerprintUnlockActiveChanged() {
            root.syncOpened()
            if (vaultController.fingerprintUnlockActive)
                root.state = 0
        }
        function onFingerprintPromptStateChanged() {
            if (vaultController.fingerprintPromptState === 0)
                root.state = 0
        }
    }
}
