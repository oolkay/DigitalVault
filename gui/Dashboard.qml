import QtQuick
import QtQuick.Controls
import DigitalVault
import QtQuick.Layouts
import QtQuick.Dialogs

Item {
    id: dashboardPage

    /* Setup bittiğinde bildirim göster */
    Connections {
        target: vaultController
        function onSetupFinished(success, message) {
            setupResultText.text = message
            setupResultText.color = success ? Theme.success : Theme.danger
            setupResultText.visible = true
        }
    }

    /* Format sırasında tüm ekranı kapatan overlay */
    Rectangle {
        id: setupOverlay
        anchors.fill: parent
        visible: vaultController.isSetupRunning
        color: Qt.rgba(0, 0, 0, 0.72)
        z: 100

        Behavior on opacity { NumberAnimation { duration: 300 } }

        Rectangle {
            anchors.centerIn: parent
            width: 360; height: 200
            radius: Theme.radius
            color: Theme.bgSurface
            border.color: Theme.border
            border.width: 1

            Column {
                anchors.centerIn: parent
                spacing: 20
                width: parent.width - 48

                /* Başlık */
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 10
                    Text {
                        text: "🔐"
                        font.pixelSize: 22
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: "Formatting Vault"
                        color: Theme.textPrimary
                        font { pixelSize: 18; bold: true; family: "Inter" }
                        anchors.verticalCenter: parent.verticalCenter
                        Behavior on color { ColorAnimation { duration: 250 } }
                    }
                }

                /* Status yazısı */
                Text {
                    width: parent.width
                    text: vaultController.setupStatusText
                    color: Theme.textSecondary
                    font { pixelSize: 11; family: "Inter" }
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    Behavior on color { ColorAnimation { duration: 250 } }
                }

                /* Animasyonlu progress bar */
                Rectangle {
                    id: progressTrack
                    width: parent.width; height: 6
                    radius: 3
                    color: Theme.bgOverlay
                    clip: true

                    Rectangle {
                        id: progressFill
                        height: parent.height
                        radius: parent.radius
                        color: Theme.accent

                        SequentialAnimation on width {
                            running: vaultController.isSetupRunning
                            loops: Animation.Infinite
                            NumberAnimation { from: 0; to: progressTrack.width; duration: 1800; easing.type: Easing.InOutQuart }
                            NumberAnimation { from: progressTrack.width; to: 0; duration: 1800; easing.type: Easing.InOutQuart }
                        }
                    }
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Do not close the application."
                    color: Theme.textMuted
                    font { pixelSize: 10; family: "Inter" }
                    Behavior on color { ColorAnimation { duration: 250 } }
                }
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

            // Title row
            RowLayout {
                width: parent.width
                Text {
                    text: "Dashboard"
                    color: Theme.textPrimary
                    font { pixelSize: 26; bold: true; family: "Inter" }
                    Behavior on color { ColorAnimation { duration: 250 } }
                }
                Item { Layout.fillWidth: true }
            }

            // ── Top stat cards ────────────────────────────────────
            Row {
                width: parent.width
                spacing: 16

                Repeater {
                    model: [
                        { label: "Vault Status",   value: vaultController.isLocked ? "Locked" : "Mounted",  icon: "🔒", accent: vaultController.isLocked ? Theme.danger : Theme.success },
                        { label: "Fingerprints",   value: vaultController.fingerprintCount + " registered", icon: "☝",  accent: Theme.accent },
                        { label: "Encryption",     value: "AES-256-XTS",  icon: "🔑", accent: Theme.warning },
                        { label: "HAL Status",     value: "Simulator",    icon: "⚡", accent: Theme.success }
                    ]
                    delegate: Rectangle {
                        width: (parent.width - 48) / 4
                        height: 90
                        radius: Theme.radius
                        color: Theme.bgCard
                        border.color: Theme.border
                        border.width: 1
                        Behavior on color { ColorAnimation { duration: 250 } }
                        Behavior on border.color { ColorAnimation { duration: 250 } }

                        // top accent line
                        Rectangle {
                            anchors { top: parent.top; left: parent.left; right: parent.right }
                            height: 3
                            radius: parent.radius
                            color: modelData.accent
                        }

                        Column {
                            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 16; topMargin: 18 }
                            spacing: 6
                            Row {
                                spacing: 8
                                Text { text: modelData.icon; font.pixelSize: 16 }
                                Text {
                                    text: modelData.label
                                    color: Theme.textSecondary
                                    font { pixelSize: 11; bold: true; family: "Inter"; letterSpacing: 0.5 }
                                    Behavior on color { ColorAnimation { duration: 250 } }
                                }
                            }
                            Text {
                                text: modelData.value
                                color: modelData.accent
                                font { pixelSize: 15; bold: true; family: "Inter" }
                            }
                        }
                    }
                }
            }

            // ── Middle row: vault control + devices ───────────────
            Row {
                width: parent.width
                height: vaultController.isLocked ? 300 : 360
                Behavior on height { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                spacing: 20

                // Vault control panel
                Rectangle {
                    width: parent.width * 0.42
                    height: parent.height
                    radius: Theme.radius
                    color: Theme.bgCard
                    border.color: Theme.border
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: 250 } }
                    Behavior on border.color { ColorAnimation { duration: 250 } }

                    Column {
                        anchors.centerIn: parent
                        spacing: 20

                        // Fingerprint ring
                        Item {
                            width: 110; height: 110
                            anchors.horizontalCenter: parent.horizontalCenter

                            // Outer glow ring
                            Rectangle {
                                anchors.centerIn: parent
                                width: 110; height: 110
                                radius: 55
                                color: "transparent"
                                border.color: vaultController.isLocked ? Theme.danger : Theme.success
                                border.width: 2
                                opacity: 0.4
                            }
                            // Inner circle
                            Rectangle {
                                anchors.centerIn: parent
                                width: 84; height: 84
                                radius: 42
                                color: vaultController.isLocked ? Theme.dangerBg : Theme.successBg
                                Behavior on color { ColorAnimation { duration: 400 } }

                                Text {
                                    anchors.centerIn: parent
                                    text: "☝"
                                    font.pixelSize: 36
                                    color: vaultController.isLocked ? Theme.danger : Theme.success
                                    Behavior on color { ColorAnimation { duration: 400 } }
                                }
                            }
                            // Pulse animation
                            SequentialAnimation on scale {
                                running: !vaultController.isLocked
                                loops: Animation.Infinite
                                NumberAnimation { to: 1.05; duration: 800; easing.type: Easing.InOutSine }
                                NumberAnimation { to: 1.00; duration: 800; easing.type: Easing.InOutSine }
                            }
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: vaultController.vaultStatusText
                            color: Theme.textPrimary
                            font { pixelSize: 17; bold: true; family: "Inter" }
                            Behavior on color { ColorAnimation { duration: 250 } }
                        }

                        // Action button
                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: 160; height: 42
                            radius: 21
                            color: vaultController.isToggling ? "#444" : (vaultController.isLocked ? Theme.success : Theme.danger)
                            Behavior on color { ColorAnimation { duration: 300 } }
                            opacity: vaultController.isToggling ? 0.8 : 1.0

                            scale: (btnMouse.containsMouse && !vaultController.isToggling) ? 1.04 : 1.0
                            Behavior on scale { NumberAnimation { duration: 150 } }

                            Text {
                                anchors.centerIn: parent
                                text: {
                                    if (vaultController.isToggling) {
                                        return vaultController.isLocked ? "⏳ Decrypting..." : "⏳ Locking..."
                                    }
                                    return vaultController.isLocked ? "🔓  Unlock Vault" : "🔒  Lock Vault"
                                }
                                color: "white"
                                font { pixelSize: 13; bold: true; family: "Inter" }
                            }
                            MouseArea {
                                id: btnMouse
                                anchors.fill: parent
                                hoverEnabled: !vaultController.isToggling
                                cursorShape: vaultController.isToggling ? Qt.ArrowCursor : Qt.PointingHandCursor
                                onClicked: if (!vaultController.isToggling) vaultController.toggleVault()
                            }
                        }

                        // SETUP BUTTON (Sadece kilitliyken ve bir cihaz seçiliyken)
                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: 160; height: 36
                            radius: 18
                            color: "transparent"
                            border.color: Theme.warning
                            border.width: 1
                            visible: vaultController.isLocked && vaultController.selectedDevice !== ""
                            
                            scale: setupMouse.containsMouse ? 1.04 : 1.0
                            Behavior on scale { NumberAnimation { duration: 150 } }

                            Row {
                                anchors.centerIn: parent
                                spacing: 8
                                Text { text: "🛠"; font.pixelSize: 14 }
                                Text {
                                    text: "Setup Vault"
                                    color: Theme.warning
                                    font { pixelSize: 12; bold: true; family: "Inter" }
                                }
                            }
                            MouseArea {
                                id: setupMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: setupWarningDialog.open()
                            }
                        }


                        // Quick access buttons (vault açıkken görünür)
                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 10
                            visible: !vaultController.isLocked
                            opacity: !vaultController.isLocked ? 1.0 : 0.0
                            Behavior on opacity { NumberAnimation { duration: 300 } }

                            // Dosya Yöneticisi butonu
                            Rectangle {
                                width: 130; height: 34
                                radius: 17
                                color: filesMouse.containsMouse ? Theme.accentHover : Theme.accent
                                Behavior on color { ColorAnimation { duration: 150 } }
                                scale: filesMouse.containsMouse ? 1.04 : 1.0
                                Behavior on scale { NumberAnimation { duration: 150 } }

                                Row {
                                    anchors.centerIn: parent
                                    spacing: 6
                                    Text { text: "📁"; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter }
                                    Text {
                                        text: "Open Files"
                                        color: "white"
                                        font { pixelSize: 12; bold: true; family: "Inter" }
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                                MouseArea {
                                    id: filesMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: vaultController.openInFileManager()
                                }
                            }

                            // Terminal butonu
                            Rectangle {
                                width: 130; height: 34
                                radius: 17
                                color: termMouse.containsMouse ? Theme.bgHover : Theme.bgOverlay
                                border.color: Theme.border
                                border.width: 1
                                Behavior on color { ColorAnimation { duration: 150 } }
                                scale: termMouse.containsMouse ? 1.04 : 1.0
                                Behavior on scale { NumberAnimation { duration: 150 } }

                                Row {
                                    anchors.centerIn: parent
                                    spacing: 6
                                    Text { text: "⌨"; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter }
                                    Text {
                                        text: "Terminal"
                                        color: Theme.textPrimary
                                        font { pixelSize: 12; bold: true; family: "Inter" }
                                        anchors.verticalCenter: parent.verticalCenter
                                        Behavior on color { ColorAnimation { duration: 250 } }
                                    }
                                }
                                MouseArea {
                                    id: termMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: vaultController.openInTerminal()
                                }
                            }
                        }
                    }
                }

                // Connected devices list
                Rectangle {
                    id: devicesPanel
                    width: parent.width - parent.children[0].width - 20
                    height: parent.height
                    radius: Theme.radius
                    color: Theme.bgCard
                    border.color: Theme.border
                    border.width: 1
                    clip: true
                    Behavior on color { ColorAnimation { duration: 250 } }
                    Behavior on border.color { ColorAnimation { duration: 250 } }

                    Column {
                        id: devicesPanelContent
                        anchors { top: parent.top; left: parent.left; right: parent.right; margins: 20 }
                        spacing: 12

                        // Header row
                        Item {
                            width: parent.width
                            height: 32

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "Connected Devices"
                                color: Theme.textPrimary
                                font { pixelSize: 15; bold: true; family: "Inter" }
                                Behavior on color { ColorAnimation { duration: 250 } }
                            }

                            Button {
                                anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                                text: "+ Create New"
                                flat: true
                                font { pixelSize: 11; bold: true }
                                palette.buttonText: Theme.accent
                                onClicked: creationDialog.open()
                            }
                        }

                        // Scrollable device list
                        ScrollView {
                            width: parent.width
                            height: devicesPanel.height - devicesPanelContent.spacing - 32 - 40
                            clip: true
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                            Column {
                                width: devicesList.width
                                spacing: 8

                                Repeater {
                                    id: devicesList
                                    model: vaultController.getStorageDevices()

                                    delegate: Rectangle {
                                        // Use the ScrollView's available width
                                        width: devicesPanel.width - 40
                                        height: 62
                                        radius: 10
                                        color: vaultController.selectedDevice === modelData.path
                                               ? Theme.accentGlow : Theme.bgOverlay
                                        border.color: vaultController.selectedDevice === modelData.path
                                                      ? Theme.accent : Theme.border
                                        border.width: vaultController.selectedDevice === modelData.path ? 2 : 1
                                        Behavior on color { ColorAnimation { duration: 200 } }
                                        Behavior on border.color { ColorAnimation { duration: 200 } }

                                        // Clickable area for selection
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                vaultController.setSelectedDevice(modelData.path)
                                            }
                                        }

                                        // Content row using anchors (not RowLayout) for reliable spacing
                                        Item {
                                            anchors { fill: parent; leftMargin: 14; rightMargin: 14 }

                                            // Device icon
                                            Rectangle {
                                                id: devIcon
                                                anchors.verticalCenter: parent.verticalCenter
                                                width: 38; height: 38; radius: 8
                                                color: modelData.path.startsWith("/dev/")
                                                       ? Theme.warningBg : Theme.accentGlow
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: modelData.path.indexOf("loop") >= 0
                                                          ? "🔁"
                                                          : (modelData.path.startsWith("/dev/") ? "💾" : "🔐")
                                                    font.pixelSize: 18
                                                }
                                            }

                                            // Status badge (right-anchored first so text can elide)
                                            Rectangle {
                                                id: statusBadge
                                                anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                                                width: badgeLabel.implicitWidth + 20
                                                height: 24; radius: 12
                                                color: vaultController.selectedDevice === modelData.path
                                                       ? Theme.successBg : "transparent"
                                                border.color: vaultController.selectedDevice === modelData.path
                                                              ? Theme.success : Theme.border
                                                border.width: 1
                                                Text {
                                                    id: badgeLabel
                                                    anchors.centerIn: parent
                                                    text: vaultController.selectedDevice === modelData.path
                                                          ? "✓  SELECTED" : "SELECT"
                                                    color: vaultController.selectedDevice === modelData.path
                                                           ? Theme.success : Theme.textMuted
                                                    font { pixelSize: 10; bold: true; family: "Inter" }
                                                }
                                            }

                                            // Name + description (fills space between icon and badge)
                                            Column {
                                                anchors {
                                                    left: devIcon.right; leftMargin: 12
                                                    right: statusBadge.left; rightMargin: 10
                                                    verticalCenter: parent.verticalCenter
                                                }
                                                spacing: 3
                                                Text {
                                                    width: parent.width
                                                    text: modelData.name
                                                    color: Theme.textPrimary
                                                    font { pixelSize: 13; bold: true; family: "Inter" }
                                                    elide: Text.ElideRight
                                                    Behavior on color { ColorAnimation { duration: 250 } }
                                                }
                                                Text {
                                                    width: parent.width
                                                    text: modelData.description
                                                    color: Theme.textSecondary
                                                    font { pixelSize: 11; family: "Inter" }
                                                    elide: Text.ElideRight
                                                    Behavior on color { ColorAnimation { duration: 250 } }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ── Activity feed ─────────────────────────────────────
            Rectangle {
                width: parent.width
                height: 140
                radius: Theme.radius
                color: Theme.bgCard
                border.color: Theme.border
                border.width: 1
                Behavior on color { ColorAnimation { duration: 250 } }
                Behavior on border.color { ColorAnimation { duration: 250 } }

                Column {
                    anchors { fill: parent; margins: 20 }
                    spacing: 12

                    Text {
                        text: "Recent Activity"
                        color: Theme.textPrimary
                        font { pixelSize: 15; bold: true; family: "Inter" }
                        Behavior on color { ColorAnimation { duration: 250 } }
                    }

                    Repeater {
                        model: vaultController.securityLogs
                        delegate: Row {
                            width: parent.width
                            spacing: 12
                            visible: index < 3 // Sadece son 3 aktiviteyi göster
                            
                            Text { 
                                text: {
                                    if (modelData.level === "SUCCESS") return "✅"
                                    if (modelData.level === "ERROR") return "❌"
                                    if (modelData.level === "WARNING") return "⚠️"
                                    return "ℹ️"
                                }
                                font.pixelSize: 14 
                            }
                            Text {
                                text: modelData.time.split(" ")[1].substring(0, 5) // HH:mm formatı
                                color: Theme.textMuted
                                font { pixelSize: 12; family: "Inter" }
                                Behavior on color { ColorAnimation { duration: 250 } }
                            }
                            Text {
                                text: modelData.message
                                color: Theme.textSecondary
                                font { pixelSize: 12; family: "Inter" }
                                width: parent.width - 80
                                elide: Text.ElideRight
                                Behavior on color { ColorAnimation { duration: 250 } }
                            }
                        }
                    }
                }
            }
        }
    }
    FileDialog {
        id: creationDialog
        title: "Create New Vault Container"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Vault Images (*.img)", "All Files (*)"]
        onAccepted: {
            vaultController.createNewVault(creationDialog.selectedFile)
        }
    }

    // Modern Step-by-Step Setup Wizard Dialog
    Dialog {
        id: setupWarningDialog
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

        contentItem: Item {
            implicitHeight: setupWizardMainCol.implicitHeight
            implicitWidth: 500
            
            Column {
                id: setupWizardMainCol
                width: parent.width
                spacing: 0

                // Top decorative bar
                Rectangle {
                    width: parent.width; height: 4
                    radius: Theme.radius
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: Theme.accent }
                        GradientStop { position: 1.0; color: Theme.warning }
                    }
                }

                Column {
                    id: wizardContentCol
                    width: parent.width
                    spacing: 20
                    topPadding: 20
                    bottomPadding: 24
                    leftPadding: 28
                    rightPadding: 28

                    // Header
                    Row {
                        width: parent.width - wizardContentCol.leftPadding - wizardContentCol.rightPadding
                        spacing: 10
                        Text { text: "🛠"; font.pixelSize: 20; anchors.verticalCenter: parent.verticalCenter }
                        Text {
                            text: "Vault Setup Wizard"
                            color: Theme.textPrimary
                            font { pixelSize: 18; bold: true; family: "Inter" }
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    // Progress Indicator
                    Row {
                        width: parent.width - wizardContentCol.leftPadding - wizardContentCol.rightPadding
                        spacing: 8
                        Rectangle {
                            width: (parent.width - 8) / 2; height: 4; radius: 2
                            color: setupWizardLayout.currentIndex >= 0 ? Theme.accent : Theme.border
                            Behavior on color { ColorAnimation { duration: 300 } }
                        }
                        Rectangle {
                            width: (parent.width - 8) / 2; height: 4; radius: 2
                            color: setupWizardLayout.currentIndex >= 1 ? Theme.accent : Theme.border
                            Behavior on color { ColorAnimation { duration: 300 } }
                        }
                    }

                    StackLayout {
                        id: setupWizardLayout
                        width: parent.width - wizardContentCol.leftPadding - wizardContentCol.rightPadding
                        height: currentItem ? currentItem.implicitHeight : 0
                        currentIndex: 0
                        Behavior on height { NumberAnimation { duration: 280; easing.type: Easing.OutCubic } }

                        // ── STEP 1: Vault Details ──
                        Item {
                            width: parent.width
                            implicitHeight: step1Col.implicitHeight
                            Column {
                                id: step1Col
                                width: parent.width
                                spacing: 16

                                Text {
                                    text: "Step 1: Vault Configuration"
                                    color: Theme.textPrimary
                                    font { pixelSize: 14; bold: true; family: "Inter" }
                                }
                                Text {
                                    text: "Configure your new vault. Choose a name and assign the Master Fingerprint ID that will have full ownership access."
                                    color: Theme.textSecondary
                                    font { pixelSize: 12; family: "Inter" }
                                    width: parent.width
                                    wrapMode: Text.Wrap
                                }

                                RowLayout {
                                    width: parent.width
                                    spacing: 16
                                    
                                    Column {
                                        spacing: 6
                                        Layout.fillWidth: true
                                        Text { text: "Vault Name"; color: Theme.textSecondary; font { pixelSize: 11; bold: true; family: "Inter" } }
                                        Rectangle {
                                            width: parent.width; height: 38; radius: 8
                                            color: Theme.bgInput
                                            border.color: Theme.border; border.width: 1
                                            TextInput {
                                                id: vaultNameInput
                                                anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                                                verticalAlignment: TextInput.AlignVCenter
                                                color: Theme.textPrimary
                                                font { pixelSize: 14; family: "Inter" }
                                                text: "My Vault"
                                            }
                                        }
                                    }

                                    Column {
                                        spacing: 6
                                        Layout.preferredWidth: 120
                                        Text { text: "Owner Finger ID"; color: Theme.textSecondary; font { pixelSize: 11; bold: true; family: "Inter" } }
                                        Rectangle {
                                            width: parent.width; height: 38; radius: 8
                                            color: Theme.bgInput
                                            border.color: Theme.border; border.width: 1
                                            TextInput {
                                                id: ownerIdInput
                                                anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                                                verticalAlignment: TextInput.AlignVCenter
                                                color: Theme.textPrimary
                                                font { pixelSize: 14; family: "Inter" }
                                                text: "1"
                                                validator: IntValidator { bottom: 1; top: 127 }
                                            }
                                        }
                                    }
                                }

                                Rectangle { width: parent.width; height: 1; color: Theme.border }
                            }
                        }

                        // ── STEP 2: Confirmation & Format ──
                        Item {
                            width: parent.width
                            implicitHeight: step2Col.implicitHeight
                            Column {
                                id: step2Col
                                width: parent.width
                                spacing: 16

                                Text {
                                    text: "Step 2: Biometric Authentication & Formatting"
                                    color: Theme.textPrimary
                                    font { pixelSize: 14; bold: true; family: "Inter" }
                                }
                                Text {
                                    text: "You are about to format the following device. This will ERASE ALL DATA. Once you click 'Start Setup', place your finger on the sensor to authenticate and begin formatting."
                                    color: Theme.textSecondary
                                    font { pixelSize: 12; family: "Inter" }
                                    width: parent.width
                                    wrapMode: Text.Wrap
                                }

                                Rectangle {
                                    width: parent.width; height: 60
                                    color: Theme.dangerBg; radius: 8; border.color: Theme.danger; border.width: 1
                                    Row {
                                        anchors.centerIn: parent; spacing: 12
                                        Text { text: "⚠"; font.pixelSize: 22; color: Theme.danger }
                                        Column {
                                            Text { text: "Target Device"; color: Theme.danger; font { pixelSize: 10; bold: true; family: "Inter" } }
                                            Text { text: vaultController.selectedDevice; color: Theme.danger; font { pixelSize: 13; bold: true; family: "Courier New" } }
                                        }
                                    }
                                }

                                // Force format checkbox
                                Rectangle {
                                    width: parent.width; height: 38; radius: 8
                                    color: forceCheckbox.checked ? Theme.dangerBg : Qt.rgba(1,1,1,0.04)
                                    border.color: forceCheckbox.checked ? Theme.danger : Theme.border
                                    border.width: 1

                                    Row {
                                        anchors.centerIn: parent; spacing: 10
                                        CheckBox { id: forceCheckbox; anchors.verticalCenter: parent.verticalCenter }
                                        Text {
                                            text: "Force format (ignore existing data)"
                                            color: forceCheckbox.checked ? Theme.danger : Theme.textPrimary
                                            font { pixelSize: 12; bold: forceCheckbox.checked; family: "Inter" }
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                    }
                                }

                                Text {
                                    id: setupResultText
                                    width: parent.width
                                    visible: false; height: visible ? implicitHeight : 0
                                    wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter
                                    font { pixelSize: 12; bold: true; family: "Inter" }
                                }

                                Rectangle { width: parent.width; height: 1; color: Theme.border }
                            }
                        }
                    }

                    // Shared Wizard Buttons (Always at the bottom right of the dialog)
                    Item {
                        width: parent.width - wizardContentCol.leftPadding - wizardContentCol.rightPadding
                        height: 40
                        
                        Row {
                            anchors.right: parent.right
                            spacing: 16

                            // Left Button (Cancel or Back)
                            Rectangle {
                                width: 100; height: 38; radius: 19
                                color: "transparent"; border.color: Theme.border; border.width: 1
                                Text { 
                                    anchors.centerIn: parent
                                    text: setupWizardLayout.currentIndex === 0 ? "Cancel" : "← Back"
                                    color: Theme.textSecondary
                                    font { pixelSize: 12; bold: true; family: "Inter" } 
                                }
                                MouseArea { 
                                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (setupWizardLayout.currentIndex === 0) setupWarningDialog.close()
                                        else setupWizardLayout.currentIndex = 0
                                    }
                                }
                            }

                            // Right Button (Next or Start Setup)
                            Rectangle {
                                width: setupWizardLayout.currentIndex === 0 ? 120 : 180; height: 38; radius: 19
                                color: setupWizardLayout.currentIndex === 0 ? Theme.accent : Theme.danger
                                Behavior on width { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                                Behavior on color { ColorAnimation { duration: 250 } }

                                Text { 
                                    anchors.centerIn: parent
                                    text: setupWizardLayout.currentIndex === 0 ? "Next →" : "☝ Start Setup"
                                    color: "white"
                                    font { pixelSize: 12; bold: true; family: "Inter" } 
                                }
                                MouseArea {
                                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (setupWizardLayout.currentIndex === 0) {
                                            if (ownerIdInput.text !== "") setupWizardLayout.currentIndex = 1
                                        } else {
                                            var oid = parseInt(ownerIdInput.text);
                                            if (isNaN(oid)) oid = 1;
                                            vaultController.setupVault(vaultController.selectedDevice, forceCheckbox.checked, oid);
                                            setupWarningDialog.close();
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        header: null
        footer: null

        onClosed: {
            forceCheckbox.checked = false
            setupResultText.visible = false
            setupWizardLayout.currentIndex = 0
            vaultNameInput.text = "My Vault"
            ownerIdInput.text = "1"
        }
    }
}
