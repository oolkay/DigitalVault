import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DigitalVault

Item {
    id: createVaultPage

    function openSetupWizard(devicePath) {
        var path = devicePath || setupDeviceList.selectedPath
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
                createVaultDialog.close()
        }
        function onDeviceDeleteFinished(success, message) {
            deleteResultLabel.text = message
            deleteResultLabel.color = success ? Theme.success : Theme.danger
            deleteResultLabel.visible = true
            if (success)
                deleteConfirmDialog.close()
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
                    text: "Create Vault"
                    color: Theme.textPrimary
                    font { pixelSize: 26; bold: true; family: "Inter" }
                }
                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: "Create a container, then set up encryption. Registered vaults appear on the Dashboard."
                    color: Theme.textSecondary
                    font { pixelSize: 13; family: "Inter" }
                }
            }

            Rectangle {
                width: parent.width
                radius: Theme.radius
                color: Theme.bgCard
                border.color: Theme.border
                border.width: 1
                implicitHeight: step1Col.implicitHeight + 40

                ColumnLayout {
                    id: step1Col
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 12

                    Text {
                        text: "1. Create container"
                        color: Theme.textPrimary
                        font { pixelSize: 16; bold: true; family: "Inter" }
                    }
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        text: "Creates a sparse image and attaches it as /dev/loopN. You will encrypt it in step 2."
                        color: Theme.textSecondary
                        font { pixelSize: 12; family: "Inter" }
                    }
                    Rectangle {
                        Layout.preferredWidth: 200
                        Layout.preferredHeight: 40
                        radius: 20
                        color: loopBtnMouse.containsMouse ? Theme.accentHover : Theme.accent
                        Text {
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
                            onClicked: createVaultDialog.open()
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width
                radius: Theme.radius
                color: Theme.bgCard
                border.color: Theme.border
                border.width: 1
                implicitHeight: step2Col.implicitHeight + 40

                Column {
                    id: step2Col
                    width: parent.width - 40
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 20
                    spacing: 12

                    Text {
                        text: "2. Set up encrypted vault"
                        color: Theme.textPrimary
                        font { pixelSize: 16; bold: true; family: "Inter" }
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: "Choose a block device without encryption yet, then run the setup wizard."
                        color: Theme.textSecondary
                        font { pixelSize: 12; family: "Inter" }
                    }

                    Column {
                        id: setupDeviceList
                        width: parent.width
                        spacing: 8
                        property string selectedPath: ""

                        Repeater {
                            id: setupDevicesRepeater
                            model: vaultController.storageDevices.filter(function(d) {
                                return !d.isVault && d.path && d.path.indexOf("/dev/") === 0
                            })
                            delegate: Rectangle {
                                width: setupDeviceList.width
                                height: 58
                                radius: 8
                                color: setupDeviceList.selectedPath === modelData.path
                                       ? Theme.accentGlow : Theme.bgOverlay
                                border.color: setupDeviceList.selectedPath === modelData.path
                                              ? Theme.accent : Theme.border
                                border.width: 1

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 10

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Text {
                                            text: modelData.name || modelData.path
                                            color: Theme.textPrimary
                                            font { pixelSize: 13; bold: true; family: "Inter" }
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            text: modelData.description || modelData.path
                                            color: Theme.textMuted
                                            font { pixelSize: 10; family: "Inter" }
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                        }
                                    }

                                    Rectangle {
                                        visible: modelData.canDelete === true
                                        Layout.preferredWidth: 28
                                        Layout.preferredHeight: 28
                                        radius: 6
                                        color: delMouse.containsMouse ? Theme.dangerBg : "transparent"
                                        border.color: Theme.danger
                                        border.width: 1
                                        Text {
                                            anchors.centerIn: parent
                                            text: "−"
                                            color: Theme.danger
                                            font { pixelSize: 16; bold: true }
                                        }
                                        MouseArea {
                                            id: delMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                deleteConfirmDialog.targetDevice =
                                                    modelData.deleteTargetPath || modelData.path
                                                deleteConfirmDialog.targetIsVault = false
                                                deleteConfirmDialog.open()
                                            }
                                        }
                                    }

                                    Rectangle {
                                        Layout.preferredWidth: 88
                                        Layout.preferredHeight: 32
                                        radius: 16
                                        color: setupBtnMouse.containsMouse ? Theme.warning : Theme.accent
                                        Text {
                                            anchors.centerIn: parent
                                            text: "Setup"
                                            color: "white"
                                            font { pixelSize: 11; bold: true; family: "Inter" }
                                        }
                                        MouseArea {
                                            id: setupBtnMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                setupDeviceList.selectedPath = modelData.path
                                                createVaultPage.openSetupWizard(modelData.path)
                                            }
                                        }
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    z: -1
                                    onClicked: setupDeviceList.selectedPath = modelData.path
                                }
                            }
                        }

                        Text {
                            width: parent.width
                            wrapMode: Text.Wrap
                            visible: setupDevicesRepeater.count === 0
                            text: "No setup-ready devices. Create a loop device above, or connect storage."
                            color: Theme.textMuted
                            font { pixelSize: 12; family: "Inter" }
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: deleteConfirmDialog
        anchors.centerIn: parent
        width: 420
        modal: true
        padding: 0

        property string targetDevice: ""
        property bool targetIsVault: false

        background: Rectangle {
            color: Theme.bgSurface
            radius: Theme.radius
            border.color: Theme.border
            border.width: 1
        }

        contentItem: Item {
            implicitWidth: deleteConfirmDialog.width
            implicitHeight: deleteDialogColumn.implicitHeight + 48

            ColumnLayout {
                id: deleteDialogColumn
                anchors.fill: parent
                anchors.margins:  24
                spacing: 16

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: "Delete DigitalVault container?"
                    color: Theme.textPrimary
                    font { pixelSize: 18; bold: true; family: "Inter" }
                }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WrapAnywhere
                    text: "Permanently detach " + deleteConfirmDialog.targetDevice
                          + " and delete its backing image."
                    color: Theme.textSecondary
                    font { pixelSize: 13; family: "Inter" }
                }
                Text {
                    id: deleteResultLabel
                    Layout.fillWidth: true
                    visible: false
                    wrapMode: Text.Wrap
                    font { pixelSize: 12; family: "Inter" }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Item { Layout.fillWidth: true }
                    WizardActionButton {
                        text: "Cancel"
                        variant: "secondary"
                        onClicked: deleteConfirmDialog.close()
                    }
                    WizardActionButton {
                        text: "Delete"
                        variant: "danger"
                        onClicked: vaultController.deleteDevice(deleteConfirmDialog.targetDevice)
                    }
                }
            }
        }
    }

    Dialog {
        id: createVaultDialog
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

        onOpened: Qt.callLater(resetCreateWizard)

        function resetCreateWizard() {
            createChrome.resultText.visible = false
            createChrome.stackLayout.currentIndex = 0
            createContainerField.text = "myvault"
            createTotalSlider.value = Math.min(1024, createTotalSlider.to)
            createStorageField.text = vaultController.defaultContainersDir()
            refreshCreateSummary()
        }

        function refreshCreateSummary() {
            createSummaryBacking.text = createStorageField.text + "/" + createContainerField.text + ".img"
            createSummaryLoop.text = "(created on confirm)"
            createSummarySize.text = (createTotalSlider.value / 1024).toFixed(1) + " GB total"
        }

        contentItem: WizardChrome {
            id: createChrome
            width: 500
            stepCount: 2
            wizardIcon: "🔁"
            wizardTitle: "Create Loop Device"
            gradientEndColor: "#7A5AF8"
            finalLabel: "Create Loop Device"
            finalButtonWidth: 200

            onBackRequested: createVaultDialog.handleBack()
            onPrimaryRequested: createVaultDialog.handlePrimary()

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
                        Text { text: "LOOP DEVICE"; color: Theme.accent; font { pixelSize: 10; bold: true } }
                        Text { id: createSummaryBacking; width: parent.width; wrapMode: Text.Wrap; color: Theme.textPrimary; font { pixelSize: 12 } }
                        Text { id: createSummaryLoop; color: Theme.textPrimary; font { pixelSize: 12 } }
                        Text { id: createSummarySize; color: Theme.textPrimary; font { pixelSize: 12 } }
                    }
                }
            }
        }

        function handleBack() {
            if (createChrome.currentIndex === 0)
                createVaultDialog.close()
            else
                createChrome.stackLayout.currentIndex = 0
        }

        function handlePrimary() {
            if (createChrome.currentIndex === 0) {
                refreshCreateSummary()
                createChrome.stackLayout.currentIndex = 1
            } else {
                vaultController.createLoopDevice(createContainerField.text, createTotalSlider.value, "")
            }
        }
    }

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
