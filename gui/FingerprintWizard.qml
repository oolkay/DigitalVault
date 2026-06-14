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

    // Macro steps for the current task, used by the stepper at the top of the card.
    function taskSteps(flow) {
        switch (flow) {
        case "setup_find_existing":
        case "setup_enroll":
        case "setup_verify":
            return ["Check", "Add", "Confirm"]
        case "setup_access_find_existing":
        case "setup_access_enroll":
            return ["Check", "Add"]
        case "grant_verify":
        case "grant_find_existing":
        case "grant_enroll":
            return ["Confirm", "Check", "Add"]
        case "revoke_verify":
            return ["Verify"]
        case "delete_verify":
            return ["Verify"]
        case "unlock":
            return ["Scan"]
        default:
            return ["Scan"]
        }
    }

    function activeStepIndex(flow) {
        switch (flow) {
        case "setup_find_existing": return 0
        case "setup_enroll": return 1
        case "setup_verify": return 2
        case "setup_access_find_existing": return 0
        case "setup_access_enroll": return 1
        case "grant_verify": return 0
        case "grant_find_existing": return 1
        case "grant_enroll": return 2
        default: return 0
        }
    }

    // Plain-language explanation of what the current scan is for.
    function purposeText(flow) {
        switch (flow) {
        case "unlock":
            return "Unlock this vault with an authorized fingerprint."
        case "setup_find_existing":
            return "Checking if this finger is already saved on the sensor."
        case "setup_enroll":
            return "New finger — saving it as the vault owner."
        case "setup_verify":
            return "Confirm with the owner finger to start setup."
        case "setup_access_find_existing":
            return "Checking if this finger is already saved. If it is, we link it; if not, we add it."
        case "setup_access_enroll":
            return "New finger — adding it to this vault."
        case "grant_verify":
            return "Confirm you can manage access for this vault."
        case "grant_find_existing":
            return "Checking if the finger you want to add is already saved."
        case "grant_enroll":
            return "New finger — adding it to this vault."
        case "revoke_verify":
            return "Confirm with an authorized finger to remove access."
        case "delete_verify":
            return "Verify with an authorized finger to delete this vault."
        default:
            return ""
        }
    }

    function awaitingFound() {
        return vaultController.fingerprintAwaitingDecision
               && vaultController.fingerprintPromptState === 1
    }

    // Short, single primary line (lean overlay).
    function primaryText() {
        if (vaultController.fingerprintAwaitingDecision)
            return awaitingFound() ? "Fingerprint found" : "No saved fingerprint found"
        const ps = vaultController.fingerprintPromptState
        if (ps === 1)
            return isEnrollFlow() ? "Fingerprint added" : "Fingerprint verified"
        if (ps === 2)
            return isEnrollFlow() ? "Fingerprint not added" : "Verification failed"
        const msg = vaultController.fingerprintUnlockMessage
        return msg.length > 0 ? msg : "Scan your finger"
    }

    // Small secondary line: scan counter, short purpose, or decision hint.
    function subText() {
        if (vaultController.fingerprintAwaitingDecision)
            return awaitingFound()
                   ? "This fingerprint is already saved and will be reused."
                   : "Add it as a new fingerprint to continue."
        const ps = vaultController.fingerprintPromptState
        if (ps !== 0)
            return ""
        const flow = vaultController.fingerprintFlow
        if (isEnrollFlow()) {
            if (vaultController.fingerprintEnrollStep >= 2)
                return "Scan 2 of 2"
            if (vaultController.fingerprintEnrollStep >= 1)
                return "Scan 1 of 2"
            return ""
        }
        if (flow === "setup_find_existing" || flow === "setup_access_find_existing"
            || flow === "grant_find_existing")
            return "If it's already saved it's reused, otherwise it's added."
        return ""
    }

    function statusColor() {
        if (vaultController.fingerprintAwaitingDecision)
            return awaitingFound() ? Theme.success : Theme.warning
        const ps = vaultController.fingerprintPromptState
        if (ps === 1)
            return Theme.success
        if (ps === 2)
            return Theme.danger
        return Theme.accent
    }

    function statusBg() {
        if (vaultController.fingerprintAwaitingDecision)
            return awaitingFound() ? Theme.successBg : Theme.warningBg
        const ps = vaultController.fingerprintPromptState
        if (ps === 1)
            return Theme.successBg
        if (ps === 2)
            return Theme.dangerBg
        return Theme.accentGlow
    }

    function iconText() {
        if (vaultController.fingerprintAwaitingDecision)
            return awaitingFound() ? "✓" : "?"
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

                Row {
                    id: stepperRow
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 6
                    property var steps: root.taskSteps(vaultController.fingerprintFlow)
                    property int activeIndex: root.activeStepIndex(vaultController.fingerprintFlow)
                    visible: steps.length > 1

                    Repeater {
                        model: stepperRow.steps
                        delegate: Row {
                            spacing: 6
                            property bool isActive: index === stepperRow.activeIndex
                            property bool isDone: index < stepperRow.activeIndex

                            Text {
                                visible: index > 0
                                anchors.verticalCenter: parent.verticalCenter
                                text: "›"
                                color: Theme.textMuted
                                font { pixelSize: 13; family: Theme.fontFamily }
                            }

                            Rectangle {
                                height: 24
                                radius: 12
                                width: pillLabel.implicitWidth + 22
                                color: parent.isActive ? Theme.accentGlow
                                       : parent.isDone ? Theme.successBg
                                       : "transparent"
                                border.width: 1
                                border.color: parent.isActive ? Theme.accent
                                              : parent.isDone ? Theme.success
                                              : Theme.border

                                Text {
                                    id: pillLabel
                                    anchors.centerIn: parent
                                    text: (parent.parent.isDone ? "✓ " : "") + modelData
                                    color: parent.parent.isActive ? Theme.accent
                                           : parent.parent.isDone ? Theme.success
                                           : Theme.textMuted
                                    font {
                                        pixelSize: 11
                                        bold: parent.parent.isActive
                                        family: Theme.fontFamily
                                    }
                                }
                            }
                        }
                    }
                }

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
                        wrapMode: Text.Wrap
                        text: root.primaryText()
                        color: Theme.textPrimary
                        font { pixelSize: 16; bold: true; family: Theme.fontFamily }
                    }

                    Text {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        visible: text.length > 0
                        text: root.subText()
                        color: Theme.textSecondary
                        font { pixelSize: 12; family: Theme.fontFamily }
                    }
                }

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 10

                    Rectangle {
                        visible: vaultController.fingerprintAwaitingDecision
                        width: addLbl.implicitWidth + 32
                        height: 36
                        radius: 18
                        color: addMouse.containsMouse ? Theme.accentHover : Theme.accent

                        Text {
                            id: addLbl
                            anchors.centerIn: parent
                            text: root.awaitingFound() ? "Continue" : "Add fingerprint"
                            color: "white"
                            font { pixelSize: 12; bold: true; family: Theme.fontFamily }
                        }

                        MouseArea {
                            id: addMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: vaultController.continueAfterDecision()
                        }
                    }

                    Rectangle {
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
    }

    Connections {
        target: vaultController
        function onFingerprintUnlockActiveChanged() { root.syncOpened() }
    }
}
