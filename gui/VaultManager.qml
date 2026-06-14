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

    function unlockVaultFromHome(devicePath) {
        if (!devicePath)
            return
        vaultController.setSelectedDevice(devicePath)
        vaultController.toggleVault()
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
            onVaultDeleteRequested: function(path) { vaultManagerPage.requestDeleteVault(path) }
            onVaultUnlockRequested: function(path) { vaultManagerPage.unlockVaultFromHome(path) }
            onVaultSetupRequested: function(path) { vaultManagerPage.openSetupWizard(path) }
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
                    font { pixelSize: 18; bold: true; family: Theme.fontFamily }
                }

                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: "Encrypted vault data will be permanently destroyed. This action cannot be undone."
                    color: Theme.textPrimary
                    font { pixelSize: 12; family: Theme.fontFamily }
                }

                Rectangle {
                    width: parent.width
                    height: deleteWarningText.implicitHeight + 20
                    radius: Theme.radiusSm
                    color: Theme.dangerBg
                    border.color: Theme.danger
                    border.width: 1

                    Text {
                        id: deleteWarningText
                        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 12; rightMargin: 12 }
                        wrapMode: Text.Wrap
                        text: "Review the affected files below, then type the confirmation phrase exactly."
                        color: Theme.textPrimary
                        font { pixelSize: 13; family: Theme.fontFamily }
                    }
                }

                StorageOutcomePreview {
                    width: parent.width
                    preview: deleteVaultDialog.preview
                }

                WizardTextField {
                    id: deleteConfirmField
                    width: parent.width
                    label: "Confirmation phrase"
                    placeholderText: deleteVaultDialog.preview.confirmHint
                                     ? ("Type " + deleteVaultDialog.preview.confirmHint)
                                     : "Type DELETE"
                    text: ""
                }

                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: deleteBtn.enabled
                          ? "Ready to delete."
                          : "Delete stays disabled until the confirmation phrase matches."
                    color: deleteBtn.enabled ? Theme.danger : Theme.textMuted
                    font { pixelSize: 13; family: Theme.fontFamily }
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
                    font { pixelSize: 16; bold: true; family: Theme.fontFamily }
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
                    font { pixelSize: 13; family: Theme.fontFamily }
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
            wizardIcon: "C"
            wizardTitle: "Create Loop Device"
            gradientEndColor: Theme.accent
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
                        from: 256
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
                        Text { text: "Backing image"; color: Theme.accent; font { pixelSize: 13; bold: true; family: Theme.fontFamily } }
                        Text { id: createSummaryBacking; width: parent.width; wrapMode: Text.Wrap; color: Theme.textPrimary; font { pixelSize: 13; family: Theme.fontFamily } }
                        Text { id: createSummarySize; color: Theme.textPrimary; font { pixelSize: 13; family: Theme.fontFamily } }
                    }
                }
            }
        }
    }

    // ── Setup wizard (migrated from Create Vault) ─────────────
    Dialog {
        id: setupWarningDialog
        anchors.centerIn: parent
        width: 560
        implicitHeight: setupChrome.implicitHeight + 24
        modal: true
        padding: 0
        z: 220
        closePolicy: Popup.CloseOnEscape

        property var setupSession: ({ valid: false })
        property int setupLogicalStep: 0
        readonly property int setupStepCount: canShowNormalStepNow() ? 4 : 3

        onOpened: Qt.callLater(resetSetupWizardFromSession)

        function canShowNormalStepNow() {
            if (!setupSession.valid)
                return false
            var remaining = setupSession.capacityMB - setupVaultSizeSlider.value
            return setupSession.canCreateNormalPartition
                   && remaining >= setupSession.minNormalMB
        }

        function wholeDeviceVaultSelected() {
            return setupSession.valid && !canShowNormalStepNow()
        }

        function stackIndexForLogical(logical) {
            if (canShowNormalStepNow())
                return logical
            return logical < 2 ? logical : 3
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
            width: 560
            stepCount: setupWarningDialog.setupStepCount
            progressStep: setupWarningDialog.setupLogicalStep
            wizardIcon: "V"
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
                                     ? "Owner fingerprint saved as " + vaultController.setupOwnerLabel + "."
                                     : "Owner fingerprint saved. Add a name before continuing.")
                                  : "Enter a name, then add the owner fingerprint."
                            color: vaultController.setupOwnerFingerId > 0 ? Theme.success : Theme.textSecondary
                            font { pixelSize: 14; family: Theme.fontFamily }
                        }
                        Rectangle {
                            visible: vaultController.setupOwnerFingerId < 1
                            width: enrollBtnLbl.implicitWidth + 28
                            height: 42
                            radius: Theme.radiusSm
                            color: enrollMouse.containsMouse ? Theme.accentHover : Theme.accent
                            Text {
                                id: enrollBtnLbl
                                anchors.centerIn: parent
                                text: "Add owner fingerprint"
                                color: "white"
                                font { pixelSize: 14; bold: true; family: Theme.fontFamily }
                            }
                            MouseArea {
                                id: enrollMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (ownerFingerprintNameField.text.trim().length === 0) {
                                        vaultController.notifyUser("Enter a fingerprint name before adding.", true)
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
                        label: "Vault size"
                        from: 128
                        to: 2048
                        value: 1024
                        hintText: setupWarningDialog.setupSession.valid
                                  && !setupWarningDialog.canShowNormalStepNow()
                                  ? "Setup will encrypt the whole device; no separate normal partition will be created."
                                  : ""
                        onValueChanged: {
                            if (!setupWarningDialog.visible)
                                return
                            if (setupWarningDialog.setupLogicalStep >= 2)
                                setupWarningDialog.applyLogicalStep(
                                    Math.min(setupWarningDialog.setupLogicalStep,
                                             setupWarningDialog.setupStepCount - 1))
                        }
                    }
                }
            }

            Item {
                width: setupChrome.stackLayout.width
                implicitHeight: setupAccessStep.implicitHeight
                Column {
                    id: setupAccessStep
                    width: parent.width
                    spacing: 14
                    WizardStepTitle { title: "Step 2: Authorized fingerprints" }

                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: "Owner access is required. Add up to three additional fingerprints now; after setup this list becomes read-only."
                        color: Theme.textSecondary
                        font { pixelSize: 13; family: Theme.fontFamily }
                    }

                    Rectangle {
                        width: parent.width
                        height: ownerAccessRow.implicitHeight + 18
                        radius: 8
                        color: Theme.bgOverlay
                        border.color: Theme.border
                        border.width: 1
                        Row {
                            id: ownerAccessRow
                            anchors.centerIn: parent
                            width: parent.width - 24
                            spacing: 10
                            Text {
                                width: Math.max(0, parent.width - ownerBadge.implicitWidth - 16)
                                text: vaultController.setupOwnerLabel.length > 0
                                      ? vaultController.setupOwnerLabel
                                      : "Owner fingerprint"
                                color: Theme.textPrimary
                                font { pixelSize: 14; bold: true; family: Theme.fontFamily }
                                elide: Text.ElideRight
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                id: ownerBadge
                                text: "Owner"
                                color: Theme.accent
                                font { pixelSize: 12; bold: true; family: Theme.fontFamily }
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: 8
                        Repeater {
                            model: vaultController.setupAccessEntries
                            delegate: Rectangle {
                                width: parent.width
                                height: 42
                                radius: 8
                                color: Theme.bgOverlay
                                border.color: Theme.border
                                border.width: 1
                                Row {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 8
                                    spacing: 8
                                    Text {
                                        width: Math.max(0, parent.width - removePendingLbl.implicitWidth - pendingBadge.implicitWidth - 44)
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.label || "Fingerprint"
                                        color: Theme.textPrimary
                                        font { pixelSize: 13; bold: true; family: Theme.fontFamily }
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        id: pendingBadge
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: "Saved when setup finishes"
                                        color: Theme.textMuted
                                        font { pixelSize: 11; family: Theme.fontFamily }
                                    }
                                    Rectangle {
                                        width: removePendingLbl.implicitWidth + 16
                                        height: 28
                                        radius: Theme.radiusSm
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: removePendingMouse.containsMouse ? Theme.dangerBg : "transparent"
                                        Text {
                                            id: removePendingLbl
                                            anchors.centerIn: parent
                                            text: "Remove"
                                            color: Theme.danger
                                            font { pixelSize: 12; bold: true; family: Theme.fontFamily }
                                        }
                                        MouseArea {
                                            id: removePendingMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: vaultController.removePendingSetupAccess(modelData.fingerId)
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Row {
                        width: parent.width
                        spacing: 10
                        WizardTextField {
                            id: setupAdditionalNameField
                            width: Math.max(0, parent.width - setupAddAccessBtn.width - parent.spacing)
                            label: "Additional fingerprint name"
                            placeholderText: "Right thumb"
                        }
                        WizardActionButton {
                            id: setupAddAccessBtn
                            width: 190
                            anchors.bottom: setupAdditionalNameField.bottom
                            text: "Add fingerprint"
                            variant: "primary"
                            enabled: vaultController.setupAccessEntries.length < 3
                            onClicked: {
                                if (setupAdditionalNameField.text.trim().length === 0) {
                                    vaultController.notifyUser("Enter a fingerprint name before adding.", true)
                                    return
                                }
                                vaultController.beginSetupAccessEnroll(setupAdditionalNameField.text.trim())
                                setupAdditionalNameField.text = ""
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: vaultController.setupAccessEntries.length >= 3
                              ? "Maximum access list size reached."
                              : "You can skip this step if only the owner should unlock the vault."
                        color: vaultController.setupAccessEntries.length >= 3 ? Theme.warning : Theme.textMuted
                        font { pixelSize: 12; family: Theme.fontFamily }
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
                        WizardStepTitle { title: "Step 3: Normal (unencrypted)" }
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
                    WizardStepTitle {
                        title: setupWarningDialog.canShowNormalStepNow()
                               ? "Step 4: Setup Summary"
                               : "Step 3: Setup Summary"
                    }
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
                        Text {
                            text: "Vault filesystem: ext4"
                            color: Theme.textPrimary
                            font { pixelSize: 12 }
                        }
                        Text {
                            text: vaultController.setupAccessEntries.length > 0
                                  ? "Additional access:"
                                  : "Additional access: none"
                            color: Theme.textPrimary
                            font { pixelSize: 12; bold: vaultController.setupAccessEntries.length > 0 }
                        }
                        Repeater {
                            model: vaultController.setupAccessEntries
                            Text {
                                text: "  - " + (modelData.label || "Fingerprint")
                                color: Theme.textPrimary
                                font { pixelSize: 12 }
                            }
                        }
                        Text {
                            visible: setupWarningDialog.canShowNormalStepNow() && normalStepLoader.item
                            text: "Normal filesystem: "
                                  + (normalStepLoader.item
                                     ? normalStepLoader.item.partitionFields.filesystemCombo.currentText
                                     : setupWarningDialog.setupSession.defaultNormalFs || "exfat")
                            color: Theme.textPrimary
                            font { pixelSize: 12 }
                        }
                        Text {
                            text: setupWarningDialog.wholeDeviceVaultSelected()
                                  ? "Vault size: entire device"
                                  : "Vault size: " + (setupVaultSizeSlider.value / 1024).toFixed(1) + " GB"
                            color: Theme.textPrimary
                            font { pixelSize: 12 }
                        }
                        Text {
                            visible: setupWarningDialog.wholeDeviceVaultSelected()
                            text: "Normal partition: none"
                            color: Theme.textPrimary
                            font { pixelSize: 12 }
                        }
                    }
                    Rectangle {
                        width: parent.width
                        height: forceConfirmText.implicitHeight + 20
                        radius: 8
                        color: forceCheckbox.checked ? Theme.dangerBg : Qt.rgba(1, 1, 1, 0.04)
                        border.color: forceCheckbox.checked ? Theme.danger : Theme.border
                        border.width: 1
                        Row {
                            anchors.centerIn: parent
                            width: parent.width - 24
                            spacing: 10
                            CheckBox { id: forceCheckbox; anchors.verticalCenter: parent.verticalCenter }
                            Text {
                                id: forceConfirmText
                                width: Math.max(0, parent.width - forceCheckbox.width - parent.spacing)
                                text: "I understand all data on " + setupWarningDialog.setupSession.devicePath + " will be ERASED."
                                color: forceCheckbox.checked ? Theme.danger : Theme.textPrimary
                                font { pixelSize: 13; bold: forceCheckbox.checked; family: Theme.fontFamily }
                                wrapMode: Text.Wrap
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
                if (setupLogicalStep === 0 && vaultController.setupOwnerFingerId < 1) {
                    vaultController.notifyUser("Enroll the owner fingerprint before continuing.", true)
                    return
                }
                if (setupLogicalStep === 0
                        && ownerFingerprintNameField.text.trim().length === 0) {
                    vaultController.notifyUser("Enter a fingerprint name before continuing.", true)
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
