import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DigitalVault

Item {
    id: vaultManagerPage
    width: parent ? parent.width : 0
    height: parent ? parent.height : 0

    property string activeDetailPath: ""
    property string selectedPartitionPath: ""
    property var currentLayout: ({})

    readonly property bool anyDialogOpen:
        deleteVaultDialog.visible || renameDialog.visible
        || createLoopDialog.visible || setupWarningDialog.visible
    readonly property bool blockingOp: vaultController.isSetupRunning

    function closeVisibleDialogs() {
        if (setupWarningDialog.visible)
            setupWarningDialog.close()
        if (deleteVaultDialog.visible)
            deleteVaultDialog.close()
        if (renameDialog.visible)
            renameDialog.close()
        if (createLoopDialog.visible)
            createLoopDialog.close()
    }

    function refreshLayout() {
        if (!activeDetailPath)
            currentLayout = {}
        else
            currentLayout = vaultController.storageLayoutForDevice(activeDetailPath)
    }

    function popToHome() {
        while (vmStack.depth > 1)
            vmStack.pop()
        activeDetailPath = ""
        selectedPartitionPath = ""
    }

    function openAddVault() {
        vmStack.push(addComp)
    }

    function openVaultDetail(path) {
        activeDetailPath = path
        selectedPartitionPath = ""
        vaultController.setSelectedDevice(path)
        refreshLayout()
        vmStack.push(detailComp)
    }

    function openSetupWizard(devicePath) {
        var p = devicePath || ""
        if (!p) {
            vaultController.notifyUser("Select a device to set up first.", true)
            return
        }
        var session = vaultController.createSetupSession(p)
        if (!session.valid) {
            vaultController.notifyUser(session.error || "Device is not suitable for setup.", true)
            return
        }
        setupWarningDialog.setupSession = session
        setupWarningDialog.open()
    }

    function requestDeleteVault(devicePath) {
        if (!devicePath)
            return
        if (vaultController.fingerprintUnlockActive || deleteVaultDialog.visible)
            return
        if (!vaultController.deleteVaultRequiresBiometric(devicePath)) {
            openDeleteVault(devicePath)
            return
        }
        if (!vaultController.isVaultUnlockedForDevice(devicePath)
                && !vaultController.sensorConnected) {
            vaultController.notifyUser(
                "Connect the fingerprint sensor before deleting a locked vault.", true)
            return
        }
        vaultController.beginDeleteVaultBiometricGate(devicePath)
    }

    function openDeleteVault(devicePath) {
        deleteVaultDialog.targetPath = devicePath
        deleteVaultDialog.preview = vaultController.previewDeleteVault(devicePath)
        deleteConfirmField.text = ""
        deleteVaultDialog.open()
    }

    function openRename(devicePath, currentName) {
        renameDialog.targetPath = devicePath
        renameNameField.text = currentName || ""
        renameDialog.open()
    }

    Item {
        id: pageContent
        anchors.fill: parent

        StackView {
            id: vmStack
            anchors.fill: parent

            pushEnter: Transition {
                PropertyAnimation { property: "opacity"; from: 0; to: 1; duration: 220; easing.type: Easing.OutCubic }
                PropertyAnimation { property: "y"; from: 10; to: 0; duration: 220; easing.type: Easing.OutCubic }
            }
            pushExit: Transition {
                PropertyAnimation { property: "opacity"; from: 1; to: 0; duration: 140 }
            }
            popEnter: Transition {
                PropertyAnimation { property: "opacity"; from: 0; to: 1; duration: 180 }
            }
            popExit: Transition {
                PropertyAnimation { property: "opacity"; from: 1; to: 0; duration: 140 }
                PropertyAnimation { property: "y"; from: 0; to: 8; duration: 140 }
            }

            Component.onCompleted: {
                vmStack.push(homeComp)
                Qt.callLater(function() {
                    const pendingPath = vaultController.takePendingVaultManagerDetailPath()
                    if (pendingPath)
                        vaultManagerPage.openVaultDetail(pendingPath)
                })
            }
        }
    }

    ModalScrim {
        anchors.fill: parent
        sourceItem: pageContent
        active: anyDialogOpen || blockingOp
        dismissible: anyDialogOpen && !blockingOp
        blocking: blockingOp
        onBackdropClicked: closeVisibleDialogs()
    }

    SetupProgressOverlay {
        anchors.fill: parent
        blurSource: pageContent
        backdropEnabled: false
    }

    Connections {
        target: vaultController
        function onSetupFinished(success, message) {
            setupChrome.resultText.text = message
            setupChrome.resultText.color = success ? Theme.success : Theme.danger
            setupChrome.resultText.visible = true
            if (success)
                vaultManagerPage.popToHome()
        }
        function onLoopCreateFinished(success, message) {
            createChrome.resultText.text = message
            createChrome.resultText.color = success ? Theme.success : Theme.danger
            createChrome.resultText.visible = true
            if (success)
                createLoopDialog.close()
            if (success)
                vaultManagerPage.popToHome()
        }
        function onDeviceDeleteFinished(success, message) {
            if (success) {
                deleteVaultDialog.close()
                vaultManagerPage.popToHome()
            }
        }
        function onDevicesChanged() {
            if (activeDetailPath)
                refreshLayout()
        }
        function onStatusChanged() {
            if (activeDetailPath)
                refreshLayout()
        }
        function onDeleteVaultGatePassed(devicePath) {
            vaultManagerPage.openDeleteVault(devicePath)
        }
    }

    Component {
        id: homeComp
        VaultManagerHome {
            onAddVaultRequested: vaultManagerPage.openAddVault()
            onVaultOpenRequested: function(path) { vaultManagerPage.openVaultDetail(path) }
        }
    }

    Component {
        id: addComp
        VaultManagerAddVault {
            onBackRequested: vmStack.pop()
            onSetupRequested: function(path) { vaultManagerPage.openSetupWizard(path) }
            onCreateLoopRequested: createLoopDialog.open()
        }
    }

    Component {
        id: detailComp
        VaultManagerVaultDetail {
            devicePath: vaultManagerPage.activeDetailPath
            layoutData: vaultManagerPage.currentLayout
            selectedPartitionPath: vaultManagerPage.selectedPartitionPath
            onPartitionPathChanged: function(path) {
                vaultManagerPage.selectedPartitionPath = path
            }
            onBackRequested: vaultManagerPage.popToHome()
            onRenameRequested: vaultManagerPage.openRename(
                vaultManagerPage.activeDetailPath,
                deviceData.displayName || deviceData.name || "")
            onDeleteRequested: vaultManagerPage.requestDeleteVault(vaultManagerPage.activeDetailPath)
            onSetupEncryptionRequested: vaultManagerPage.openSetupWizard(vaultManagerPage.activeDetailPath)
        }
    }

    // ── Delete vault (unified preview) ────────────────────────
    Dialog {
        id: deleteVaultDialog
        anchors.centerIn: parent
        width: 480
        modal: true
        padding: 0
        z: 220
        closePolicy: Popup.CloseOnEscape

        property string targetPath: ""
        property var preview: ({})

        background: Rectangle {
            color: Theme.bgSurface
            radius: Theme.radius
            border.color: Theme.danger
            border.width: 1
        }

        onClosed: {
            deleteConfirmField.text = ""
            preview = ({})
        }

        contentItem: Item {
            implicitWidth: deleteVaultDialog.width
            implicitHeight: deleteDialogCol.implicitHeight + 48

            Column {
                id: deleteDialogCol
                anchors.fill: parent
                anchors.margins: 24
                spacing: 12

                Text {
                    width: parent.width
                    text: "Delete vault?"
                    color: Theme.danger
                    font { pixelSize: 18; bold: true; family: "Inter" }
                }

                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: "Encrypted vault data will be permanently destroyed. This cannot be undone."
                    color: Theme.textSecondary
                    font { pixelSize: 12; family: "Inter" }
                }

                StorageOutcomePreview {
                    width: parent.width
                    preview: deleteVaultDialog.preview
                }

                WizardTextField {
                    id: deleteConfirmField
                    width: parent.width
                    label: "Type vault name or DELETE to confirm"
                    text: ""
                }

                Item {
                    width: parent.width
                    height: 36

                    WizardActionButton {
                        id: deleteBtn
                        anchors.right: parent.right
                        text: "Delete vault"
                        variant: "danger"
                        enabled: {
                            var hint = (deleteVaultDialog.preview.confirmHint || "DELETE").toUpperCase()
                            var typed = deleteConfirmField.text.trim().toUpperCase()
                            return typed === "DELETE" || typed === hint.toUpperCase()
                        }
                        onClicked: {
                            const path = deleteVaultDialog.targetPath
                            deleteVaultDialog.close()
                            vaultController.deleteVault(path)
                        }
                    }

                    WizardActionButton {
                        id: cancelBtn
                        anchors.right: deleteBtn.left
                        anchors.rightMargin: 12
                        text: "Cancel"
                        variant: "secondary"
                        onClicked: deleteVaultDialog.close()
                    }
                }
            }
        }
    }

    // ── Rename display name ───────────────────────────────────
    Dialog {
        id: renameDialog
        anchors.centerIn: parent
        width: 400
        modal: true
        padding: 0
        z: 220
        closePolicy: Popup.CloseOnEscape

        property string targetPath: ""

        background: Rectangle {
            color: Theme.bgSurface
            radius: Theme.radius
            border.color: Theme.border
            border.width: 1
        }

        contentItem: Item {
            implicitWidth: renameDialog.width
            implicitHeight: renameDialogCol.implicitHeight + 48

            Column {
                id: renameDialogCol
                anchors.fill: parent
                anchors.margins: 24
                spacing: 12

                Text {
                    width: parent.width
                    text: "Rename vault (display only)"
                    color: Theme.textPrimary
                    font { pixelSize: 16; bold: true; family: "Inter" }
                }

                WizardTextField {
                    id: renameNameField
                    width: parent.width
                    label: "Display name"
                    text: ""
                }

                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: "Device path: " + renameDialog.targetPath + " (unchanged)"
                    color: Theme.textMuted
                    font { pixelSize: 11; family: "Inter" }
                }

                Item {
                    width: parent.width
                    height: 36

                    WizardActionButton {
                        id: renameSaveBtn
                        anchors.right: parent.right
                        text: "Save"
                        variant: "primary"
                        onClicked: {
                            if (vaultController.renameVaultDisplayName(
                                    renameDialog.targetPath, renameNameField.text))
                                renameDialog.close()
                        }
                    }

                    WizardActionButton {
                        id: renameCancelBtn
                        anchors.right: renameSaveBtn.left
                        anchors.rightMargin: 12
                        text: "Cancel"
                        variant: "secondary"
                        onClicked: renameDialog.close()
                    }
                }
            }
        }
    }

    // ── Create loop wizard ────────────────────────────────────
    Dialog {
        id: createLoopDialog
        anchors.centerIn: parent
        width: 500
        modal: true
        padding: 0
        z: 220
        closePolicy: Popup.CloseOnEscape

        background: Rectangle {
            color: Theme.bgSurface
            radius: Theme.radius
            border.color: Theme.border
            border.width: 1
        }

        onOpened: Qt.callLater(function() {
            createChrome.resultText.visible = false
            createChrome.stackLayout.currentIndex = 0
            createContainerField.text = "myvault"
            createTotalSlider.value = Math.min(1024, createTotalSlider.to)
            createStorageField.text = vaultController.defaultContainersDir()
            createSummaryBacking.text = createStorageField.text + "/" + createContainerField.text + ".img"
            createSummarySize.text = (createTotalSlider.value / 1024).toFixed(1) + " GB total"
        })

        contentItem: WizardChrome {
            id: createChrome
            width: 500
            stepCount: 2
            wizardIcon: "🔁"
            wizardTitle: "Create Loop Device"
            gradientEndColor: "#7A5AF8"
            finalLabel: "Create Loop Device"
            finalButtonWidth: 200

            onBackRequested: {
                if (createChrome.stackLayout.currentIndex === 0)
                    createLoopDialog.close()
                else
                    createChrome.stackLayout.currentIndex = 0
            }
            onPrimaryRequested: {
                if (createChrome.stackLayout.currentIndex === 0) {
                    createSummaryBacking.text = createStorageField.text + "/"
                        + createContainerField.text + ".img"
                    createSummarySize.text = (createTotalSlider.value / 1024).toFixed(1) + " GB total"
                    createChrome.stackLayout.currentIndex = 1
                } else {
                    vaultController.createLoopDevice(
                        createContainerField.text, createTotalSlider.value, "")
                }
            }

            Item {
                width: createChrome.stackLayout.width
                implicitHeight: createStep1.implicitHeight
                Column {
                    id: createStep1
                    width: parent.width
                    spacing: 16
                    WizardStepTitle { title: "Step 1: Container" }
                    WizardTextField {
                        id: createContainerField
                        width: parent.width
                        label: "Device name"
                        text: "myvault"
                    }
                    WizardSizeSlider {
                        id: createTotalSlider
                        width: parent.width
                        label: "Total disk size"
                        from: 16
                        to: 8192
                        value: 1024
                    }
                    WizardTextField {
                        id: createStorageField
                        width: parent.width
                        label: "Storage location"
                        readOnly: true
                        text: vaultController.defaultContainersDir()
                    }
                }
            }

            Item {
                width: createChrome.stackLayout.width
                implicitHeight: createStep2.implicitHeight
                Column {
                    id: createStep2
                    width: parent.width
                    spacing: 16
                    WizardStepTitle { title: "Step 2: Summary" }
                    WizardSummaryBox {
                        width: parent.width
                        Text { text: "Backing image"; color: Theme.accent; font { pixelSize: 10; bold: true } }
                        Text { id: createSummaryBacking; width: parent.width; wrapMode: Text.Wrap; color: Theme.textPrimary; font { pixelSize: 12 } }
                        Text { id: createSummarySize; color: Theme.textPrimary; font { pixelSize: 12 } }
                    }
                }
            }
        }
    }

    // ── Setup wizard (migrated from Create Vault) ─────────────
    Dialog {
        id: setupWarningDialog
        anchors.centerIn: parent
        width: 500
        implicitHeight: setupChrome.implicitHeight + 24
        modal: true
        padding: 0
        z: 220
        closePolicy: Popup.CloseOnEscape

        property var setupSession: ({ valid: false })
        property int setupLogicalStep: 0
        readonly property int setupStepCount: canShowNormalStepNow() ? 3 : 2

        onOpened: Qt.callLater(resetSetupWizardFromSession)

        function canShowNormalStepNow() {
            if (!setupSession.valid)
                return false
            var remaining = setupSession.capacityMB - setupVaultSizeSlider.value
            return setupSession.canCreateNormalPartition
                   && remaining >= setupSession.minNormalMB
        }

        function stackIndexForLogical(logical) {
            if (canShowNormalStepNow())
                return logical
            return logical === 0 ? 0 : 2
        }

        function applyLogicalStep(logical) {
            setupLogicalStep = logical
            if (setupChrome)
                setupChrome.stackLayout.currentIndex = stackIndexForLogical(logical)
        }

        function resetSetupWizardFromSession() {
            if (!setupChrome || !setupVaultSizeSlider || !setupSession.valid)
                return
            setupChrome.resultText.visible = false
            vaultNameField.text = setupSession.defaultVaultName || "My Vault"
            ownerFingerprintNameField.text = vaultController.setupOwnerLabel || ""
            if (setupSession.devicePath)
                vaultController.prepareSetupOwnerSlot(setupSession.devicePath)
            if (normalStepLoader.item) {
                normalStepLoader.item.partitionFields.normalName =
                    setupSession.defaultNormalName || "Normal"
                normalStepLoader.item.partitionFields.filesystemCombo.currentIndex = 0
            }
            if (forceCheckbox)
                forceCheckbox.checked = setupSession.requiresForceConfirmation || false
            setupVaultSizeSlider.from = setupSession.minVaultMB
            setupVaultSizeSlider.to = setupSession.maxVaultMB
            setupVaultSizeSlider.value = setupSession.defaultVaultMB
            setupVaultSizeSlider.syncTextFromValue()
            applyLogicalStep(0)
        }

        background: Rectangle {
            color: Theme.bgSurface
            radius: Theme.radius
            border.color: Theme.border
            border.width: 1
        }

        contentItem: WizardChrome {
            id: setupChrome
            width: 500
            stepCount: setupWarningDialog.setupStepCount
            progressStep: setupWarningDialog.setupLogicalStep
            wizardIcon: "🔐"
            wizardTitle: "Vault Setup Wizard"
            gradientEndColor: Theme.warning
            finalLabel: "Start Setup"
            finalButtonDanger: true
            finalButtonWidth: 180
            primaryEnabled: setupWarningDialog.setupLogicalStep !== setupWarningDialog.setupStepCount - 1
                              || forceCheckbox.checked

            onBackRequested: setupWarningDialog.handleBack()
            onPrimaryRequested: setupWarningDialog.handlePrimary()

            Item {
                width: setupChrome.stackLayout.width
                implicitHeight: setupStep1.implicitHeight
                Column {
                    id: setupStep1
                    width: parent.width
                    spacing: 16
                    WizardStepTitle { title: "Step 1: Vault (encrypted)" }
                    WizardTextField {
                        id: vaultNameField
                        width: parent.width
                        label: "Vault Name"
                        text: "My Vault"
                    }
                    Column {
                        width: parent.width
                        spacing: 8
                        WizardFieldLabel { label: "Owner fingerprint" }
                        WizardTextField {
                            id: ownerFingerprintNameField
                            width: parent.width
                            label: "Fingerprint name"
                            placeholderText: "Left Hand Point"
                            text: vaultController.setupOwnerLabel
                            onTextChanged: vaultController.setupOwnerLabel = text
                        }
                        Text {
                            width: parent.width
                            wrapMode: Text.Wrap
                            text: vaultController.setupOwnerFingerId > 0
                                  ? (vaultController.setupOwnerLabel.length > 0
                                     ? "Owner fingerprint registered as " + vaultController.setupOwnerLabel + "."
                                     : "Owner fingerprint registered. Add a name before continuing.")
                                  : "Enter a name, then enroll the owner fingerprint."
                            color: vaultController.setupOwnerFingerId > 0 ? Theme.success : Theme.textSecondary
                            font { pixelSize: 12; family: "Inter" }
                        }
                        Rectangle {
                            width: enrollBtnLbl.implicitWidth + 28
                            height: 36
                            radius: 18
                            color: enrollMouse.containsMouse ? Theme.accentHover : Theme.accent
                            Text {
                                id: enrollBtnLbl
                                anchors.centerIn: parent
                                text: vaultController.setupOwnerFingerId > 0
                                      ? "Re-enroll owner" : "Enroll owner fingerprint"
                                color: "white"
                                font { pixelSize: 12; bold: true; family: "Inter" }
                            }
                            MouseArea {
                                id: enrollMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (ownerFingerprintNameField.text.trim().length === 0) {
                                        vaultController.notifyUser("Enter a fingerprint name before enrolling.", true)
                                        return
                                    }
                                    vaultController.setupOwnerLabel = ownerFingerprintNameField.text.trim()
                                    vaultController.beginSetupOwnerEnrollForDevice(
                                        setupWarningDialog.setupSession.devicePath)
                                }
                            }
                        }
                    }
                    WizardSizeSlider {
                        id: setupVaultSizeSlider
                        width: parent.width
                        label: "Vault partition size"
                        from: 16
                        to: 2048
                        value: 1024
                        hintText: setupWarningDialog.setupSession.valid
                                  && !setupWarningDialog.canShowNormalStepNow()
                                  ? "Device is small — setup will encrypt the whole device (no separate normal partition)."
                                  : ""
                        onValueChanged: {
                            if (!setupWarningDialog.visible)
                                return
                            if (setupWarningDialog.setupLogicalStep
                                    >= setupWarningDialog.setupStepCount)
                                setupWarningDialog.applyLogicalStep(
                                    setupWarningDialog.setupStepCount - 1)
                        }
                    }
                }
            }

            Item {
                width: setupChrome.stackLayout.width
                implicitHeight: normalStepLoader.active ? normalStepLoader.implicitHeight : 0
                Loader {
                    id: normalStepLoader
                    active: setupWarningDialog.canShowNormalStepNow()
                    width: parent.width
                    sourceComponent: normalStepComponent
                }
                Component {
                    id: normalStepComponent
                    Column {
                        width: setupChrome.stackLayout.width
                        spacing: 16
                        property alias partitionFields: setupPartitionFields
                        WizardStepTitle { title: "Step 2: Normal (unencrypted)" }
                        WizardPartitionFields {
                            id: setupPartitionFields
                            width: parent.width
                        }
                    }
                }
            }

            Item {
                width: setupChrome.stackLayout.width
                implicitHeight: setupStep3.implicitHeight
                Column {
                    id: setupStep3
                    width: parent.width
                    spacing: 16
                    WizardStepTitle { title: "Step 3: Setup Summary" }
                    WizardSummaryBox {
                        width: parent.width
                        Text { text: "Target: " + setupWarningDialog.setupSession.devicePath; color: Theme.textPrimary; font { pixelSize: 12 } }
                        Text { text: "Vault: " + vaultNameField.text; color: Theme.textPrimary; font { pixelSize: 12 } }
                        Text {
                            text: vaultController.setupOwnerFingerId > 0
                                  ? "Owner fingerprint: " + vaultController.setupOwnerLabel
                                  : "Owner fingerprint: not enrolled yet"
                            color: Theme.textPrimary
                            font { pixelSize: 12 }
                        }
                        Text { text: "Size: " + (setupVaultSizeSlider.value / 1024).toFixed(1) + " GB"; color: Theme.textPrimary; font { pixelSize: 12 } }
                    }
                    Rectangle {
                        width: parent.width
                        height: 38
                        radius: 8
                        color: forceCheckbox.checked ? Theme.dangerBg : Qt.rgba(1, 1, 1, 0.04)
                        border.color: forceCheckbox.checked ? Theme.danger : Theme.border
                        border.width: 1
                        Row {
                            anchors.centerIn: parent
                            spacing: 10
                            CheckBox { id: forceCheckbox; anchors.verticalCenter: parent.verticalCenter }
                            Text {
                                text: "I understand all data on " + setupWarningDialog.setupSession.devicePath + " will be ERASED."
                                color: forceCheckbox.checked ? Theme.danger : Theme.textPrimary
                                font { pixelSize: 11; bold: forceCheckbox.checked; family: "Inter" }
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }
            }
        }

        function handleBack() {
            if (setupLogicalStep === 0) {
                setupWarningDialog.close()
                return
            }
            applyLogicalStep(setupLogicalStep - 1)
        }

        function handlePrimary() {
            if (setupLogicalStep < setupStepCount - 1) {
                if (setupLogicalStep === 0
                        && ownerFingerprintNameField.text.trim().length === 0) {
                    vaultController.notifyUser("Enter a fingerprint name before continuing.", true)
                    return
                }
                if (setupLogicalStep === 0 && vaultController.setupOwnerFingerId < 1) {
                    vaultController.notifyUser("Enroll the owner fingerprint before continuing.", true)
                    return
                }
                vaultController.setupOwnerLabel = ownerFingerprintNameField.text.trim()
                applyLogicalStep(setupLogicalStep + 1)
                return
            }
            if (ownerFingerprintNameField.text.trim().length === 0) {
                vaultController.notifyUser("Enter a fingerprint name before setup.", true)
                return
            }
            if (vaultController.setupOwnerFingerId < 1) {
                vaultController.notifyUser("Enroll the owner fingerprint before setup.", true)
                return
            }
            vaultController.setupOwnerLabel = ownerFingerprintNameField.text.trim()
            if (!forceCheckbox.checked) {
                vaultController.notifyUser(
                    "You must confirm that all data on the device will be erased.", true)
                return
            }
            var normalName = setupSession.defaultNormalName || "Normal"
            var normalFs = setupSession.defaultNormalFs || "exfat"
            if (canShowNormalStepNow() && normalStepLoader.item) {
                normalName = normalStepLoader.item.partitionFields.normalName
                normalFs = normalStepLoader.item.partitionFields.filesystemCombo.currentText
            }
            var vaultPartMb = setupWarningDialog.canShowNormalStepNow()
                              ? parseInt(setupVaultSizeSlider.value) : 0
            vaultController.beginVaultSetupBiometric(
                setupSession.devicePath,
                setupSession.requiresForceConfirmation,
                vaultPartMb,
                vaultNameField.text,
                normalName,
                normalFs
            )
            setupWarningDialog.close()
        }

        onClosed: {
            vaultController.clearSetupWizardSession()
            setupSession = { valid: false }
            setupLogicalStep = 0
            if (setupChrome)
                setupChrome.resetToFirstStep()
            if (forceCheckbox)
                forceCheckbox.checked = false
        }
    }
}
