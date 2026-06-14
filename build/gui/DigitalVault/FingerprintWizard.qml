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

    function syncOpened() {
        if (vaultController.fingerprintUnlockActive) {
            if (!opened)
                open()
        } else if (opened) {
            close()
        }
    }

    Component.onCompleted: syncOpened()

    function isEnrollFlow() {
        return vaultController.fingerprintFlow === "setup_enroll"
               || vaultController.fingerprintFlow === "setup_access_enroll"
               || vaultController.fingerprintFlow === "grant_enroll"
    }

    function flowTitle(flow) {
        switch (flow) {
        case "setup_find_existing": return "Check existing fingerprint"
        case "setup_enroll": return "Add owner fingerprint"
        case "setup_verify": return "Confirm setup"
        case "setup_access_find_existing": return "Check setup fingerprint"
        case "setup_access_enroll": return "Add setup fingerprint"
        case "grant_verify": return "Confirm current access"
        case "grant_find_existing": return "Scan fingerprint to add"
        case "grant_enroll": return "Add new fingerprint"
        case "revoke_verify": return "Confirm removal"
        case "delete_verify": return "Confirm deletion"
        case "unlock": return "Unlock vault"
        default: return "Fingerprint"
        }
    }

    function headlineText() {
        const ps = vaultController.fingerprintPromptState
        if (ps === 1)
            return isEnrollFlow() ? "Fingerprint added" : "Fingerprint verified"
        if (ps === 2)
            return isEnrollFlow() ? "Fingerprint not added" : "Verification failed"
        return flowTitle(vaultController.fingerprintFlow)
    }

    function statusText() {
        const ps = vaultController.fingerprintPromptState
        const msg = vaultController.fingerprintUnlockMessage
        if (ps === 1)
            return isEnrollFlow() ? "Complete" : "Verified"
        if (ps === 2)
            return "Try again"
        if (!isEnrollFlow())
            return "Waiting for touch"

        const lower = msg.toLowerCase()
        if (lower.indexOf("remove") >= 0)
            return "Lift finger"
        if (vaultController.fingerprintEnrollStep >= 2)
            return "Second scan"
        if (vaultController.fingerprintEnrollStep >= 1)
            return "First scan"
        return "Get ready"
    }

    function helperText() {
        const ps = vaultController.fingerprintPromptState
        if (ps === 1)
            return "You can continue."
        if (ps === 2)
            return "Cancel and start again when you are ready."
        const flow = vaultController.fingerprintFlow
        if (flow === "grant_verify")
            return "This confirms you can manage access for this vault."
        if (flow === "grant_find_existing")
            return "If this fingerprint is already saved, it will be linked to this vault."
        if (flow === "grant_enroll")
            return "This is the new fingerprint being added. Use the same finger for both scans."
        if (flow === "setup_access_find_existing")
            return "If this fingerprint is already saved, it will be added to this vault setup."
        if (flow === "setup_access_enroll")
            return "This fingerprint will be saved only if setup completes."
        if (isEnrollFlow())
            return "Use the same finger for both scans."
        return "Touch the sensor with an authorized finger."
    }

    function statusColor() {
        const ps = vaultController.fingerprintPromptState
        if (ps === 1)
            return Theme.success
        if (ps === 2)
            return Theme.danger
        return Theme.accent
    }

    function statusBg() {
        const ps = vaultController.fingerprintPromptState
        if (ps === 1)
            return Theme.successBg
        if (ps === 2)
            return Theme.dangerBg
        return Theme.accentGlow
    }

    function iconText() {
        const ps = vaultController.fingerprintPromptState
        if (ps === 1)
            return "✓"
        if (ps === 2)
            return "!"
        return "☝"
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
            width: Math.min(460, Math.max(320, parent.width - 48))
            radius: Theme.radius
            color: Theme.bgCard
            border.color: Theme.border
            border.width: 1
            implicitHeight: col.implicitHeight + 44

            Column {
                id: col
                anchors.centerIn: parent
                width: parent.width - 44
                spacing: 16

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 64
                    height: 64
                    radius: 32
                    color: statusBg()
                    border.color: statusColor()
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: iconText()
                        color: statusColor()
                        font { pixelSize: 28; bold: true; family: Theme.fontFamily }
                    }
                }

                Column {
                    width: parent.width
                    spacing: 6

                    Text {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        text: headlineText()
                        color: Theme.textPrimary
                        font { pixelSize: 18; bold: true; family: Theme.fontFamily }
                        wrapMode: Text.Wrap
                    }

                    Text {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        text: vaultController.fingerprintUnlockMessage
                        color: Theme.textPrimary
                        font { pixelSize: 14; bold: true; family: Theme.fontFamily }
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 44
                    radius: 8
                    color: Theme.bgOverlay
                    border.color: Theme.border
                    border.width: 1

                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        spacing: 10

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 110
                            text: statusText()
                            color: statusColor()
                            font { pixelSize: 12; bold: true; family: Theme.fontFamily }
                            elide: Text.ElideRight
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: Math.max(0, parent.width - 120)
                            wrapMode: Text.Wrap
                            text: helperText()
                            color: Theme.textSecondary
                            font { pixelSize: 12; family: Theme.fontFamily }
                        }
                    }
                }

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: cancelLbl.implicitWidth + 28
                    height: 36
                    radius: 18
                    color: cancelMouse.containsMouse ? Theme.bgHover : Theme.bgOverlay
                    border.color: Theme.border
                    border.width: 1

                    Text {
                        id: cancelLbl
                        anchors.centerIn: parent
                        text: "Cancel"
                        color: Theme.textPrimary
                        font { pixelSize: 12; bold: true; family: Theme.fontFamily }
                    }

                    MouseArea {
                        id: cancelMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: vaultController.cancelFingerprintUnlock()
                    }
                }
            }
        }
    }

    Connections {
        target: vaultController
        function onFingerprintUnlockActiveChanged() { root.syncOpened() }
    }
}
