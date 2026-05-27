import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DigitalVault

Item {
    id: vaultManagerPage

    property string selectedManagedPath: ""
    property string selectedPartitionPath: ""
    property var currentLayout: ({})

    function refreshLayout() {
        if (!selectedManagedPath)
            currentLayout = {}
        else
            currentLayout = vaultController.storageLayoutForDevice(selectedManagedPath)
    }

    function openSetupWizard(devicePath) {
        var path = devicePath || ""
        if (!path) {
            vaultController.notifyUser("Select a device to set up first.", true)
            return
        }
        var session = vaultController.createSetupSession(path)
        if (!session.valid) {
            vaultController.notifyUser(session.error || "Device is not suitable for setup.", true)
            return
        }
        setupWarningDialog.setupSession = session
        setupWarningDialog.open()
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

    SetupProgressOverlay { anchors.fill: parent }

    Connections {
        target: vaultController
        function onSetupFinished(success, message) {
            setupChrome.resultText.text = message
            setupChrome.resultText.color = success ? Theme.success : Theme.danger
            setupChrome.resultText.visible = true
        }
        function onLoopCreateFinished(success, message) {
            createChrome.resultText.text = message
            createChrome.resultText.color = success ? Theme.success : Theme.danger
            createChrome.resultText.visible = true
            if (success)
                createLoopDialog.close()
        }
        function onDeviceDeleteFinished(success, message) {
            if (success) {
                deleteVaultDialog.close()
                if (selectedManagedPath === deleteVaultDialog.targetPath)
                    selectedManagedPath = ""
                refreshLayout()
            }
        }
        function onDevicesChanged() {
            refreshLayout()
        }
        function onStatusChanged() {
            if (selectedManagedPath !== "")
                refreshLayout()
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

            Column {
                width: parent.width
                spacing: 6
                Text {
                    text: "Vault Manager"
                    color: Theme.textPrimary
                    font { pixelSize: 26; bold: true; family: "Inter" }
                }
                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: "Set up storage, edit layout, rename vaults, or delete encrypted data. Daily unlock lives on the Dashboard."
                    color: Theme.textSecondary
                    font { pixelSize: 13; family: "Inter" }
                }
            }

            // ── Connected storage ─────────────────────────────
            Column {
                width: parent.width
                spacing: 10
                Text {
                    text: "Connected storage"
                    color: Theme.textPrimary
                    font { pixelSize: 16; bold: true; family: "Inter" }
                }
                Repeater {
                    model: vaultController.discoveredStorageDevices
                    delegate: DiscoveredStorageRow {
                        width: parent.width
                        deviceData: modelData
                        onSetupRequested: vaultManagerPage.openSetupWizard(modelData.path)
                    }
                }
                Text {
                    width: parent.width
                    visible: vaultController.discoveredStorageDevices.length === 0
                    text: "No additional disks detected"
                    color: Theme.textMuted
                    font { pixelSize: 12; family: "Inter" }
                }
            }

            // ── Managed vaults ────────────────────────────────
            Column {
                width: parent.width
                spacing: 10
                Row {
                    width: parent.width
                    spacing: 12
                    Text {
                        text: "Managed vaults"
                        color: Theme.textPrimary
                        font { pixelSize: 16; bold: true; family: "Inter" }
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Unlock on Dashboard →"
                        color: Theme.accent
                        font { pixelSize: 11; family: "Inter" }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: vaultController.navigateToPage("Dashboard.qml")
                        }
                    }
                }

                Repeater {
                    model: vaultController.managedDevices
                    delegate: ManagedDeviceRow {
                        width: parent.width
                        deviceData: modelData
                        isSelected: vaultManagerPage.selectedManagedPath === modelData.path
                        onRowClicked: {
                            vaultManagerPage.selectedManagedPath = modelData.path
                            vaultManagerPage.selectedPartitionPath = ""
                            vaultManagerPage.refreshLayout()
                            vaultController.setSelectedDevice(modelData.path)
                        }
                        onRenameRequested: vaultManagerPage.openRename(
                            modelData.path, modelData.displayName)
                        onDeleteRequested: vaultManagerPage.openDeleteVault(modelData.path)
                    }
                }

                Text {
                    width: parent.width
                    visible: vaultController.managedDevices.length === 0
                    text: "No managed vaults yet. Connect a disk or create a virtual container below."
                    color: Theme.textMuted
                    font { pixelSize: 12; family: "Inter" }
                    wrapMode: Text.Wrap
                }
            }

            // ── Layout editor (selected managed) ──────────────
            Rectangle {
                width: parent.width
                visible: selectedManagedPath !== ""
                radius: Theme.radius
                color: Theme.bgCard
                border.color: Theme.border
                border.width: 1
                implicitHeight: layoutCol.implicitHeight + 32

                Column {
                    id: layoutCol
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 14

                    Text {
                        text: "Layout editor"
                        color: Theme.textPrimary
                        font { pixelSize: 14; bold: true; family: "Inter" }
                    }

                    PartitionLayoutView {
                        width: parent.width
                        layoutData: vaultManagerPage.currentLayout
                        selectedPartitionPath: vaultManagerPage.selectedPartitionPath
                        onPartitionSelected: function(path) {
                            vaultManagerPage.selectedPartitionPath = path
                        }
                    }

                    PartitionLayoutTable {
                        width: parent.width
                        visible: (vaultManagerPage.currentLayout.partitions || []).length > 0
                        partitions: vaultManagerPage.currentLayout.partitions || []
                        selectedPartitionPath: vaultManagerPage.selectedPartitionPath
                        onRowClicked: function(path) {
                            vaultManagerPage.selectedPartitionPath = path
                        }
                    }

                    StorageActionPanel {
                        width: parent.width
                        actions: vaultController.availablePartitionActions(
                            selectedManagedPath, selectedPartitionPath)
                        onActionTriggered: function(actionId) {
                            if (actionId === "setup_encryption") {
                                openSetupWizard(selectedManagedPath)
                                return
                            }
                            if (actionId === "delete_vault") {
                                openDeleteVault(selectedManagedPath)
                                return
                            }
                            vaultController.notifyUser(
                                "This action will be available in a future update.", false)
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 36
                        radius: 18
                        color: delLayoutMouse.containsMouse ? Theme.dangerBg : Theme.dangerBg
                        border.color: Theme.danger
                        Text {
                            anchors.centerIn: parent
                            text: "Delete vault"
                            color: Theme.danger
                            font { pixelSize: 12; bold: true; family: "Inter" }
                        }
                        MouseArea {
                            id: delLayoutMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: openDeleteVault(selectedManagedPath)
                        }
                    }
                }
            }

            // ── Virtual containers (v1.1 section) ─────────────
            Rectangle {
                width: parent.width
                radius: Theme.radius
                color: Theme.bgCard
                border.color: Theme.border
                border.width: 1
                implicitHeight: virtualCol.implicitHeight + 32

                Column {
                    id: virtualCol
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 10

                    Text {
                        text: "Virtual containers"
                        color: Theme.textPrimary
                        font { pixelSize: 14; bold: true; family: "Inter" }
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: "Sparse image + loop device. Encrypt via Set up encryption on the managed entry."
                        color: Theme.textSecondary
                        font { pixelSize: 11; family: "Inter" }
                    }
                    Rectangle {
                        width: loopBtnLbl.implicitWidth + 28
                        height: 36
                        radius: 18
                        color: loopBtnMouse.containsMouse ? Theme.accentHover : Theme.accent
                        Text {
                            id: loopBtnLbl
                            anchors.centerIn: parent
                            text: "+ Create loop device"
                            color: "white"
                            font { pixelSize: 12; bold: true; family: "Inter" }
                        }
                        MouseArea {
                            id: loopBtnMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: createLoopDialog.open()
                        }
                    }
                }
            }
        }
    }

    // ── Delete vault (unified preview) ────────────────────────
    Dialog {
        id: deleteVaultDialog
        anchors.centerIn: parent
        width: 480
        modal: true
        padding: 0

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
                        onClicked: vaultController.deleteVault(deleteVaultDialog.targetPath)
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
            ownerIdField.text = String(setupSession.defaultOwnerId || 1)
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
                        spacing: 6
                        WizardFieldLabel { label: "Owner Finger ID" }
                        Rectangle {
                            width: parent.width
                            height: 38
                            radius: 8
                            color: Theme.bgInput
                            border.color: Theme.border
                            border.width: 1
                            TextInput {
                                id: ownerIdField
                                anchors.fill: parent
                                anchors.margins: 12
                                verticalAlignment: TextInput.AlignVCenter
                                color: Theme.textPrimary
                                font { pixelSize: 14; family: "Inter" }
                                text: "1"
                                validator: IntValidator { bottom: 1; top: 127 }
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
                applyLogicalStep(setupLogicalStep + 1)
                return
            }
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
            vaultController.setupVault(
                setupSession.devicePath,
                setupSession.requiresForceConfirmation,
                parseInt(ownerIdField.text),
                parseInt(setupVaultSizeSlider.value),
                vaultNameField.text,
                normalName,
                normalFs
            )
            setupWarningDialog.close()
        }

        onClosed: {
            setupSession = { valid: false }
            setupLogicalStep = 0
            if (setupChrome)
                setupChrome.resetToFirstStep()
            if (forceCheckbox)
                forceCheckbox.checked = false
        }
    }
}
