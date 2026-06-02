import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DigitalVault

Item {
    id: accessPage

    property var accessEntries: []
    property bool isLoading: false

    function refreshSlots() {
        isLoading = true
        accessEntries = vaultController.accessListForDevice(vaultController.selectedDevice)
        isLoading = false
    }

    Component.onCompleted: refreshSlots()

    StackView.onActivated: refreshSlots()

    Connections {
        target: vaultController
        function onFingerprintUnlockActiveChanged() {
            if (!vaultController.fingerprintUnlockActive) {
                refreshSlots()
                if (wizardLayout)
                    wizardLayout.currentStep = 0
            }
        }
    }

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

            // ── Header ──────────────────────────────────────────────
            Column {
                width: parent.width
                spacing: 6
                Text {
                    text: "Access Manager"
                    color: Theme.textPrimary
                    font { pixelSize: 26; bold: true; family: "Inter" }
                    Behavior on color { ColorAnimation { duration: 250 } }
                }
                Text {
                    text: "Manage who can unlock the selected vault."
                    color: Theme.textSecondary
                    font { pixelSize: 13; family: "Inter" }
                    width: parent.width
                    wrapMode: Text.Wrap
                    Behavior on color { ColorAnimation { duration: 250 } }
                }
            }

            // ── No Device Selected Warning ─────────────────────────
            Rectangle {
                width: parent.width
                height: 70
                radius: Theme.radius
                color: Theme.warningBg
                border.color: Theme.warning
                border.width: 1
                visible: vaultController.selectedDevice === ""

                Row {
                    anchors { centerIn: parent }
                    spacing: 12
                    Text { text: "⚠"; font.pixelSize: 22; anchors.verticalCenter: parent.verticalCenter }
                    Text {
                        text: "Please select a vault from the Dashboard first."
                        color: Theme.warning
                        font { pixelSize: 14; bold: true; family: "Inter" }
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            // ── Grant Access Wizard Card ───────────────────────────────────
            Rectangle {
                id: wizardCard
                width: parent.width
                height: wizardLayout.implicitHeight + 40
                radius: Theme.radius
                color: Theme.bgCard
                border.color: Theme.border
                border.width: 1
                visible: vaultController.selectedDevice !== ""
                Behavior on color { ColorAnimation { duration: 250 } }
                Behavior on border.color { ColorAnimation { duration: 250 } }
                Behavior on height { NumberAnimation { duration: 300; easing.type: Easing.OutQuart } }

                // Top accent
                Rectangle {
                    anchors { top: parent.top; left: parent.left; right: parent.right }
                    height: 3; radius: parent.radius
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: "#2ECC88" }
                        GradientStop { position: 1.0; color: "#4E6BF5" }
                    }
                }

                Column {
                    id: wizardLayout
                    anchors { fill: parent; margins: 20; topMargin: 24 }
                    spacing: 20

                    property int currentStep: 0
                    property int verifiedOwnerId: -1

                    Row {
                        spacing: 10
                        Text { text: "🔑"; font.pixelSize: 18; anchors.verticalCenter: parent.verticalCenter }
                        Text {
                            text: "Grant New Access Wizard"
                            color: Theme.textPrimary
                            font { pixelSize: 15; bold: true; family: "Inter" }
                            anchors.verticalCenter: parent.verticalCenter
                            Behavior on color { ColorAnimation { duration: 250 } }
                        }
                    }

                    // Progress indicators
                    Row {
                        width: parent.width
                        spacing: 8
                        Rectangle {
                            width: (parent.width - 8) / 2; height: 4; radius: 2
                            color: wizardLayout.currentStep >= 0 ? Theme.accent : Theme.border
                            Behavior on color { ColorAnimation { duration: 300 } }
                        }
                        Rectangle {
                            width: (parent.width - 8) / 2; height: 4; radius: 2
                            color: wizardLayout.currentStep >= 1 ? Theme.accent : Theme.border
                            Behavior on color { ColorAnimation { duration: 300 } }
                        }
                    }

                    StackLayout {
                        width: parent.width
                        currentIndex: wizardLayout.currentStep

                        // ── STEP 1: Owner Authorization ──
                        Item {
                            id: step1
                            implicitHeight: step1Col.implicitHeight
                            Column {
                                id: step1Col
                                width: parent.width
                                spacing: 16

                                Text {
                                    text: "Step 1: Owner Authorization"
                                    color: Theme.textPrimary
                                    font { pixelSize: 14; bold: true; family: "Inter" }
                                }
                                Text {
                                    text: "To grant access to someone else, first verify your identity with the owner fingerprint."
                                    color: Theme.textSecondary
                                    font { pixelSize: 12; family: "Inter" }
                                    width: parent.width
                                    wrapMode: Text.Wrap
                                }

                                Row {
                                    spacing: 16
                                    Column {
                                        spacing: 6
                                        Text { text: "Owner fingerprint"; color: Theme.textSecondary; font { pixelSize: 11; bold: true; family: "Inter" } }
                                        Rectangle {
                                            width: 100; height: 38; radius: 8
                                            color: Theme.bgInput
                                            border.color: Theme.border
                                            border.width: 1
                                            TextInput {
                                                id: step1OwnerInput
                                                anchors { fill: parent; margins: 10 }
                                                color: Theme.textPrimary
                                                font { pixelSize: 14; family: "Inter" }
                                                text: "1"
                                                validator: IntValidator { bottom: 1; top: 127 }
                                            }
                                        }
                                    }

                                    Rectangle {
                                        anchors.bottom: parent.bottom
                                        width: 160; height: 38; radius: 19
                                        color: Theme.accent
                                        Text {
                                            anchors.centerIn: parent
                                            text: "Verify & Continue →"
                                            color: "white"
                                            font { pixelSize: 12; bold: true; family: "Inter" }
                                        }
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                var oid = parseInt(step1OwnerInput.text)
                                                if (isNaN(oid)) return
                                                if (vaultController.verifyOwner(oid)) {
                                                    wizardLayout.verifiedOwnerId = oid
                                                    wizardLayout.currentStep = 1
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // ── STEP 2: New User Details ──
                        Item {
                            id: step2
                            implicitHeight: step2Col.implicitHeight
                            Column {
                                id: step2Col
                                width: parent.width
                                spacing: 16

                                Text {
                                    text: "Step 2: New User Enrollment"
                                    color: Theme.textPrimary
                                    font { pixelSize: 14; bold: true; family: "Inter" }
                                }
                                Text {
                                    text: "Authorization successful. Now, enter the details for the new user. When you click 'Grant Access', the new user must place their finger on the sensor to finalize."
                                    color: Theme.success
                                    font { pixelSize: 12; family: "Inter" }
                                    width: parent.width
                                    wrapMode: Text.Wrap
                                }

                                RowLayout {
                                    width: parent.width
                                    spacing: 16
                                    
                                    Column {
                                        spacing: 6
                                        Layout.preferredWidth: 100
                                        Text { text: "New fingerprint"; color: Theme.textSecondary; font { pixelSize: 11; bold: true; family: "Inter" } }
                                        Rectangle {
                                            width: parent.width; height: 38; radius: 8
                                            color: Theme.bgInput
                                            border.color: Theme.border
                                            border.width: 1
                                            TextInput {
                                                id: step2NewIdInput
                                                anchors { fill: parent; margins: 10 }
                                                color: Theme.textPrimary
                                                font { pixelSize: 14; family: "Inter" }
                                                text: "2"
                                                validator: IntValidator { bottom: 1; top: 127 }
                                            }
                                        }
                                    }

                                    Column {
                                        spacing: 6
                                        Layout.fillWidth: true
                                        Text { text: "Alias / Description (e.g., 'Left Hand Point')"; color: Theme.textSecondary; font { pixelSize: 11; bold: true; family: "Inter" } }
                                        Rectangle {
                                            width: parent.width; height: 38; radius: 8
                                            color: Theme.bgInput
                                            border.color: Theme.border
                                            border.width: 1
                                            TextInput {
                                                id: step2NewNameInput
                                                anchors { fill: parent; margins: 10 }
                                                color: Theme.textPrimary
                                                font { pixelSize: 14; family: "Inter" }
                                                text: "New User"
                                            }
                                        }
                                    }
                                }

                                Row {
                                    spacing: 16
                                    Rectangle {
                                        width: 100; height: 38; radius: 19
                                        color: "transparent"
                                        border.color: Theme.border; border.width: 1
                                        Text {
                                            anchors.centerIn: parent
                                            text: "← Back"
                                            color: Theme.textSecondary
                                            font { pixelSize: 12; bold: true; family: "Inter" }
                                        }
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: wizardLayout.currentStep = 0
                                        }
                                    }
                                    Rectangle {
                                        width: 200; height: 38; radius: 19
                                        color: Theme.success
                                        Text {
                                            anchors.centerIn: parent
                                            text: "Scan Finger & Grant Access"
                                            color: "white"
                                            font { pixelSize: 12; bold: true; family: "Inter" }
                                        }
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                var nid = parseInt(step2NewIdInput.text)
                                                if (isNaN(nid)) return
                                                vaultController.beginGrantAccessEnroll(
                                                    wizardLayout.verifiedOwnerId,
                                                    nid,
                                                    step2NewNameInput.text)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ── Active Keyslots ─────────────────────────────────────
            Rectangle {
                width: parent.width
                height: slotsCol.implicitHeight + 40
                radius: Theme.radius
                color: Theme.bgCard
                border.color: Theme.border
                border.width: 1
                visible: vaultController.selectedDevice !== ""
                Behavior on color { ColorAnimation { duration: 250 } }
                Behavior on border.color { ColorAnimation { duration: 250 } }

                Column {
                    id: slotsCol
                    anchors { fill: parent; margins: 20 }
                    spacing: 14

                    // Header
                    Item {
                        width: parent.width
                        height: 36

                        Row {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 10
                            Text { text: "🔑"; font.pixelSize: 18; anchors.verticalCenter: parent.verticalCenter }
                            Text {
                                text: "Access list"
                                color: Theme.textPrimary
                                font { pixelSize: 15; bold: true; family: "Inter" }
                                anchors.verticalCenter: parent.verticalCenter
                                Behavior on color { ColorAnimation { duration: 250 } }
                            }
                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: countLabel.implicitWidth + 16; height: 22; radius: 11
                                color: Theme.accent
                                Text {
                                    id: countLabel
                                    anchors.centerIn: parent
                                    text: accessPage.accessEntries.length
                                    color: "white"
                                    font { pixelSize: 11; bold: true }
                                }
                            }
                        }

                        Rectangle {
                            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                            width: refreshSlotLabel.implicitWidth + 20; height: 30; radius: 15
                            color: refreshSlotMouse.containsMouse ? Theme.bgHover : "transparent"
                            border.color: Theme.border; border.width: 1
                            Behavior on color { ColorAnimation { duration: 150 } }

                            Text {
                                id: refreshSlotLabel
                                anchors.centerIn: parent
                                text: "↻  Refresh"
                                color: Theme.textSecondary
                                font { pixelSize: 11; bold: true; family: "Inter" }
                                Behavior on color { ColorAnimation { duration: 250 } }
                            }
                            MouseArea {
                                id: refreshSlotMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: refreshSlots()
                            }
                        }
                    }

                    // Slot list
                    Column {
                        width: parent.width
                        spacing: 8

                        Repeater {
                            model: accessPage.accessEntries

                            delegate: Rectangle {
                                width: parent.width
                                height: 64
                                radius: 12
                                color: modelData.isOwner ? Theme.accentGlow : Theme.bgOverlay
                                border.color: modelData.isOwner ? Theme.accent : Theme.border
                                border.width: modelData.isOwner ? 2 : 1
                                Behavior on color { ColorAnimation { duration: 200 } }

                                Item {
                                    anchors { fill: parent; leftMargin: 16; rightMargin: 16 }

                                    // Slot icon
                                    Rectangle {
                                        id: slotIcon
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 40; height: 40; radius: 10
                                        color: modelData.isOwner ? Theme.accentGlow : Theme.warningBg
                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.isOwner ? "O" : "K"
                                            font.pixelSize: 18
                                        }
                                    }

                                    // Label + type
                                    Column {
                                        anchors {
                                            left: slotIcon.right; leftMargin: 14
                                            verticalCenter: parent.verticalCenter
                                        }
                                        spacing: 3
                                        Text {
                                            text: modelData.label
                                            color: Theme.textPrimary
                                            font { pixelSize: 13; bold: true; family: "Inter" }
                                            Behavior on color { ColorAnimation { duration: 250 } }
                                        }
                                        Text {
                                            text: modelData.isOwner ? "Owner fingerprint" : "Additional fingerprint"
                                            color: Theme.textMuted
                                            font { pixelSize: 11; family: "Inter" }
                                            Behavior on color { ColorAnimation { duration: 250 } }
                                        }
                                    }

                                    // Revoke button (not for owner)
                                    Rectangle {
                                        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                                        width: revokeLabel.implicitWidth + 20; height: 28; radius: 14
                                        color: modelData.isOwner ? "transparent" : (revokeMouse.containsMouse ? Theme.dangerBg : "transparent")
                                        border.color: modelData.isOwner ? "transparent" : Theme.danger
                                        border.width: modelData.isOwner ? 0 : 1
                                        visible: !modelData.isOwner
                                        Behavior on color { ColorAnimation { duration: 150 } }

                                        Text {
                                            id: revokeLabel
                                            anchors.centerIn: parent
                                            text: "Revoke"
                                            color: Theme.danger
                                            font { pixelSize: 11; bold: true; family: "Inter" }
                                        }
                                        MouseArea {
                                            id: revokeMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                revokeConfirmDialog.slotNum = modelData.number
                                                revokeConfirmDialog.open()
                                            }
                                        }
                                    }

                                    // Owner badge
                                    Rectangle {
                                        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                                        width: ownerBadge.implicitWidth + 20; height: 24; radius: 12
                                        color: Theme.successBg
                                        border.color: Theme.success
                                        border.width: 1
                                        visible: modelData.isOwner

                                        Text {
                                            id: ownerBadge
                                            anchors.centerIn: parent
                                            text: "OWNER"
                                            color: Theme.success
                                            font { pixelSize: 10; bold: true; family: "Inter" }
                                        }
                                    }
                                }
                            }
                        }

                        // Empty state
                        Rectangle {
                            width: parent.width
                            height: 80
                            radius: 12
                            color: Theme.bgOverlay
                            border.color: Theme.border
                            border.width: 1
                            visible: accessPage.accessEntries.length === 0
                            Behavior on color { ColorAnimation { duration: 250 } }

                            Column {
                                anchors.centerIn: parent
                                spacing: 6
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "L"
                                    font.pixelSize: 22
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "No authorized fingerprints yet. Set up your vault first."
                                    color: Theme.textMuted
                                    font { pixelSize: 13; family: "Inter" }
                                    Behavior on color { ColorAnimation { duration: 250 } }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ── Revoke Confirmation Dialog ──────────────────────────────────────
    Dialog {
        id: revokeConfirmDialog
        anchors.centerIn: parent
        width: 380
        modal: true

        property int slotNum: -1

        background: Rectangle {
            color: Theme.bgSurface
            radius: Theme.radius
            border.color: Theme.border
            border.width: 1
        }

        header: Rectangle {
            width: parent.width; height: 50; color: "transparent"
            Text {
                anchors.centerIn: parent
                text: "Revoke Access"
                color: Theme.danger
                font { pixelSize: 16; bold: true; family: "Inter" }
            }
        }

        Column {
            width: parent.width
            spacing: 14
            topPadding: 10
            bottomPadding: 10

            Text {
                width: parent.width
                text: "Are you sure you want to remove this fingerprint?"
                color: Theme.textPrimary
                font { pixelSize: 14; family: "Inter" }
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }
            Text {
                width: parent.width
                text: "This user will no longer be able to unlock the vault. This action cannot be undone."
                color: Theme.textSecondary
                font { pixelSize: 12; family: "Inter" }
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }
        }

        footer: Item {
            width: parent.width; height: 60
            Row {
                anchors.centerIn: parent
                spacing: 16
                Button {
                    text: "Cancel"
                    onClicked: revokeConfirmDialog.close()
                    flat: true
                    palette.buttonText: Theme.textMuted
                }
                Button {
                    contentItem: Text {
                        text: "Revoke Access"
                        color: "white"
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        implicitWidth: 160; implicitHeight: 36
                        radius: 18
                        color: parent.pressed ? "#ff4757" : (parent.hovered ? "#ff6b7a" : Theme.danger)
                    }
                    onClicked: {
                        vaultController.revokeAccess(1, revokeConfirmDialog.slotNum)
                        revokeConfirmDialog.close()
                        refreshSlots()
                    }
                }
            }
        }
    }
}
