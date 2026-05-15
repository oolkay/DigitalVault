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

                        // Action button (Unlock/Lock or Setup)
                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: 180; height: 44
                            radius: 22
                            color: {
                                if (vaultController.isToggling) return "#444"
                                if (!vaultController.isSelectedDeviceVault) return Theme.warning
                                return vaultController.isLocked ? Theme.success : Theme.danger
                            }
                            Behavior on color { ColorAnimation { duration: 300 } }
                            opacity: (vaultController.isToggling || vaultController.selectedDevice === "") ? 0.6 : 1.0

                            scale: (btnMouse.containsMouse && !vaultController.isToggling) ? 1.04 : 1.0
                            Behavior on scale { NumberAnimation { duration: 150 } }

                            Text {
                                anchors.centerIn: parent
                                text: {
                                    if (vaultController.isToggling) {
                                        return vaultController.isLocked ? "⏳ Decrypting..." : "⏳ Locking..."
                                    }
                                    if (!vaultController.isSelectedDeviceVault) {
                                        return "🛠  Setup Vault"
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
                                onClicked: {
                                    if (vaultController.isToggling || vaultController.selectedDevice === "") return
                                    if (vaultController.isSelectedDeviceVault) {
                                        vaultController.toggleVault()
                                    } else {
                                        setupWarningDialog.open()
                                    }
                                }
                            }
                        }

                        // Re-format button for existing vaults
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "Re-format / Setup again"
                            color: Theme.textMuted
                            font { pixelSize: 11; underline: true; family: "Inter" }
                            visible: vaultController.isSelectedDeviceVault && vaultController.isLocked
                            opacity: formatMouse.containsMouse ? 1.0 : 0.7
                            
                            MouseArea {
                                id: formatMouse
                                anchors.fill: parent
                                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
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
                                width: devicesPanel.width - 40
                                spacing: 16

                                // SECTION 1: Vaults Found
                                Column {
                                    width: parent.width
                                    spacing: 8
                                    Text {
                                        text: "VAULTS FOUND"
                                        color: Theme.success
                                        font { pixelSize: 10; bold: true; family: "Inter"; letterSpacing: 1.0 }
                                        visible: vaultsRepeater.count > 0
                                    }
                                    Repeater {
                                        id: vaultsRepeater
                                        model: {
                                            var devs = vaultController.getStorageDevices()
                                            return devs.filter(d => d.isVault)
                                        }
                                        delegate: deviceDelegate
                                    }
                                }

                                // SECTION 2: Other Storage
                                Column {
                                    width: parent.width
                                    spacing: 8
                                    Text {
                                        text: "OTHER STORAGE"
                                        color: Theme.textMuted
                                        font { pixelSize: 10; bold: true; family: "Inter"; letterSpacing: 1.0 }
                                        visible: otherRepeater.count > 0
                                    }
                                    Repeater {
                                        id: otherRepeater
                                        model: {
                                            var devs = vaultController.getStorageDevices()
                                            return devs.filter(d => !d.isVault)
                                        }
                                        delegate: deviceDelegate
                                    }
                                }

                                Component {
                                    id: deviceDelegate
                                    Rectangle {
                                        width: devicesPanel.width - 40
                                        height: 62; radius: 10
                                        color: vaultController.selectedDevice === modelData.path
                                               ? Theme.accentGlow : (modelData.isVault ? Theme.successBg : Theme.bgOverlay)
                                        border.color: vaultController.selectedDevice === modelData.path
                                                      ? Theme.accent : (modelData.isVault ? Theme.success : Theme.border)
                                        border.width: vaultController.selectedDevice === modelData.path ? 2 : 1
                                        Behavior on color { ColorAnimation { duration: 200 } }
                                        Behavior on border.color { ColorAnimation { duration: 200 } }

                                        MouseArea {
                                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                            onClicked: vaultController.setSelectedDevice(modelData.path)
                                        }

                                        Item {
                                            anchors { fill: parent; leftMargin: 14; rightMargin: 14 }
                                            Rectangle {
                                                id: devIcon
                                                anchors.verticalCenter: parent.verticalCenter
                                                width: 38; height: 38; radius: 8
                                                color: modelData.isVault ? Theme.successBg : (modelData.path.startsWith("/dev/") ? Theme.warningBg : Theme.accentGlow)
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: modelData.isVault ? "🔐" : (modelData.path.indexOf("loop") >= 0 ? "🔁" : (modelData.path.startsWith("/dev/") ? "💾" : "📁"))
                                                    font.pixelSize: 18
                                                }
                                            }
                                            Rectangle {
                                                id: statusBadge
                                                anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                                                width: badgeLabel.implicitWidth + 20; height: 24; radius: 12
                                                color: vaultController.selectedDevice === modelData.path ? Theme.successBg : "transparent"
                                                border.color: vaultController.selectedDevice === modelData.path ? Theme.success : Theme.border
                                                border.width: 1
                                                Text {
                                                    id: badgeLabel; anchors.centerIn: parent
                                                    text: vaultController.selectedDevice === modelData.path ? "✓ SELECTED" : (modelData.isVault ? "USE VAULT" : "SELECT")
                                                    color: vaultController.selectedDevice === modelData.path ? Theme.success : Theme.textMuted
                                                    font { pixelSize: 10; bold: true; family: "Inter" }
                                                }
                                            }
                                            Column {
                                                anchors { 
                                                    left: devIcon.right; 
                                                    leftMargin: 12; 
                                                    right: statusBadge.left; 
                                                    rightMargin: 10; 
                                                    verticalCenter: parent.verticalCenter 
                                                }
                                                spacing: 3
                                                Text { 
                                                    width: parent.width
                                                    text: modelData.name
                                                    color: modelData.isVault ? Theme.success : Theme.textPrimary
                                                    font { pixelSize: 13; bold: true; family: "Inter" }
                                                    elide: Text.ElideRight 
                                                }
                                                Text { 
                                                    width: parent.width
                                                    text: modelData.description
                                                    color: Theme.textSecondary
                                                    font { pixelSize: 11; family: "Inter" }
                                                    elide: Text.ElideRight 
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
                        Repeater {
                            model: 3
                            Rectangle {
                                width: (parent.width - 16) / 3; height: 4; radius: 2
                                color: setupWizardLayout.currentIndex >= index ? Theme.accent : Theme.border
                                Behavior on color { ColorAnimation { duration: 300 } }
                            }
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
                                
                                RowLayout {
                                    width: parent.width; spacing: 16
                                    Column {
                                        spacing: 6; Layout.fillWidth: true
                                        Text { text: "Vault Name"; color: Theme.textSecondary; font { pixelSize: 11; bold: true; family: "Inter" } }
                                        Rectangle {
                                            width: parent.width; height: 38; radius: 8; color: Theme.bgInput; border.color: Theme.border; border.width: 1
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
                                        spacing: 6; Layout.preferredWidth: 120
                                        Text { text: "Owner Finger ID"; color: Theme.textSecondary; font { pixelSize: 11; bold: true; family: "Inter" } }
                                        Rectangle {
                                            width: parent.width; height: 38; radius: 8; color: Theme.bgInput; border.color: Theme.border; border.width: 1
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

                                Column {
                                    spacing: 12; width: parent.width
                                    visible: vaultController.selectedDevice.startsWith("/dev/")
                                    Text { text: "Vault Storage Size"; color: Theme.textSecondary; font { pixelSize: 11; bold: true; family: "Inter" } }
                                    RowLayout {
                                        width: parent.width; spacing: 16
                                        Slider {
                                            id: vaultSizeSlider; Layout.fillWidth: true; from: 64
                                            to: Math.max(64, vaultController.selectedDeviceCapacityMB - 128)
                                            value: Math.min(2048, to); stepSize: 128
                                        }
                                        Rectangle {
                                            width: 100; height: 38; radius: 8; color: Theme.bgInput; border.color: Theme.border; border.width: 1
                                            Text { anchors.centerIn: parent; text: vaultSizeSlider.value >= 1024 ? (vaultSizeSlider.value / 1024).toFixed(1) + " GB" : vaultSizeSlider.value.toFixed(0) + " MB"; color: Theme.accent; font { pixelSize: 13; bold: true; family: "Inter" } }
                                        }
                                    }
                                    Text {
                                        text: "Selected: " + (vaultSizeSlider.value / 1024).toFixed(1) + " GB | Total Device: " + (vaultController.selectedDeviceCapacityMB / 1024).toFixed(1) + " GB"
                                        color: Theme.textMuted; font { pixelSize: 10; family: "Inter" }
                                    }
                                }
                                Rectangle { width: parent.width; height: 1; color: Theme.border }
                            }
                        }

                        // ── STEP 2: Normal Partition (Conditional) ──
                        Item {
                            width: parent.width
                            implicitHeight: step2Col.implicitHeight
                            Column {
                                id: step2Col
                                width: parent.width
                                spacing: 16

                                Text {
                                    text: "Step 2: Secondary Partition"
                                    color: Theme.textPrimary
                                    font { pixelSize: 14; bold: true; family: "Inter" }
                                }
                                Text {
                                    text: "The remaining " + ((vaultController.selectedDeviceCapacityMB - vaultSizeSlider.value) / 1024).toFixed(1) + " GB will be formatted as a standard storage partition."
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
                                        Text { 
                                            text: "Partition Name"
                                            color: Theme.textSecondary
                                            font { pixelSize: 11; bold: true; family: "Inter" } 
                                        }
                                        Rectangle {
                                            width: parent.width
                                            height: 38
                                            radius: 8
                                            color: Theme.bgInput
                                            border.color: Theme.border
                                            border.width: 1
                                            TextInput { 
                                                id: normalNameInput
                                                anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                                                verticalAlignment: TextInput.AlignVCenter
                                                color: Theme.textPrimary
                                                font { pixelSize: 14; family: "Inter" }
                                                text: "Normal" 
                                            }
                                        }
                                    }
                                    Column {
                                        spacing: 6
                                        Layout.preferredWidth: 140
                                        Text { 
                                            text: "Filesystem"
                                            color: Theme.textSecondary
                                            font { pixelSize: 11; bold: true; family: "Inter" } 
                                        }
                                        ComboBox {
                                            id: normalFsCombo
                                            model: ["exfat", "vfat", "ext4"]
                                            Layout.fillWidth: true
                                            currentIndex: 0
                                        }
                                    }
                                }
                                Rectangle { 
                                    width: parent.width
                                    height: 1
                                    color: Theme.border 
                                }
                            }
                        }

                        // ── STEP 3: Summary ──
                        Item {
                            width: parent.width
                            implicitHeight: step3Col.implicitHeight
                            Column {
                                id: step3Col
                                width: parent.width
                                spacing: 16

                                Text {
                                    text: "Step 3: Setup Summary"
                                    color: Theme.textPrimary
                                    font { pixelSize: 14; bold: true; family: "Inter" }
                                }

                                Rectangle {
                                    width: parent.width
                                    height: summaryCol.implicitHeight + 24
                                    color: Qt.rgba(1,1,1,0.03)
                                    radius: 10
                                    border.color: Theme.border
                                    border.width: 1
                                    Column {
                                        id: summaryCol
                                        anchors { fill: parent; margins: 12 }
                                        spacing: 8
                                        Text { 
                                            text: "VAULT CONFIGURATION"
                                            color: Theme.accent
                                            font { pixelSize: 10; bold: true; family: "Inter" } 
                                        }
                                        Text { 
                                            text: "• Target Device: " + vaultController.selectedDevice
                                            color: Theme.textPrimary
                                            font { pixelSize: 12; family: "Inter" } 
                                        }
                                        Text { 
                                            text: "• Vault Name: " + vaultNameInput.text
                                            color: Theme.textPrimary
                                            font { pixelSize: 12; family: "Inter" } 
                                        }
                                        Text { 
                                            text: "• Vault Size: " + (vaultSizeSlider.value / 1024).toFixed(1) + " GB"
                                            color: Theme.textPrimary
                                            font { pixelSize: 12; family: "Inter" } 
                                        }
                                        Text { 
                                            text: "• Owner ID: " + ownerIdInput.text
                                            color: Theme.textPrimary
                                            font { pixelSize: 12; family: "Inter" } 
                                        }
                                        
                                        Item { 
                                            width: 1
                                            height: 4 
                                        }
                                        Text { 
                                            text: "NORMAL PARTITION"
                                            color: Theme.warning
                                            font { pixelSize: 10; bold: true; family: "Inter" }
                                            visible: vaultSizeSlider.value < vaultSizeSlider.to
                                        }
                                        Text { 
                                            text: "• Name: " + normalNameInput.text
                                            color: Theme.textPrimary
                                            font { pixelSize: 12; family: "Inter" }
                                            visible: vaultSizeSlider.value < vaultSizeSlider.to
                                        }
                                        Text { 
                                            text: "• Filesystem: " + normalFsCombo.currentText
                                            color: Theme.textPrimary
                                            font { pixelSize: 12; family: "Inter" }
                                            visible: vaultSizeSlider.value < vaultSizeSlider.to
                                        }
                                    }
                                }

                                Rectangle {
                                    width: parent.width; height: 38; radius: 8
                                    color: forceCheckbox.checked ? Theme.dangerBg : Qt.rgba(1,1,1,0.04)
                                    border.color: forceCheckbox.checked ? Theme.danger : Theme.border
                                    border.width: 1
                                    Row {
                                        anchors.centerIn: parent
                                        spacing: 10
                                        CheckBox { id: forceCheckbox; anchors.verticalCenter: parent.verticalCenter }
                                        Text {
                                            text: "I understand all data on " + vaultController.selectedDevice + " will be ERASED."
                                            color: forceCheckbox.checked ? Theme.danger : Theme.textPrimary
                                            font { pixelSize: 11; bold: forceCheckbox.checked; family: "Inter" }
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Text {
                        id: setupResultText
                        width: parent.width
                        visible: false
                        height: visible ? implicitHeight : 0
                        wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignHCenter
                        font { pixelSize: 12; bold: true; family: "Inter" }
                        color: Theme.danger
                    }

                    // Shared Wizard Buttons
                    Item {
                        width: parent.width - wizardContentCol.leftPadding - wizardContentCol.rightPadding
                        height: 40
                        
                        Row {
                            anchors.right: parent.right
                            spacing: 16

                            // Back Button
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
                                    onClicked: setupWarningDialog.handleBack()
                                }
                            }

                            // Next/Start Button
                            Rectangle {
                                width: setupWizardLayout.currentIndex === 2 ? 180 : 120; height: 38; radius: 19
                                color: setupWizardLayout.currentIndex === 2 ? Theme.danger : Theme.accent
                                Behavior on width { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                                Behavior on color { ColorAnimation { duration: 250 } }

                                Text { 
                                    anchors.centerIn: parent
                                    text: setupWizardLayout.currentIndex === 2 ? "☝ Start Setup" : "Next →"
                                    color: "white"
                                    font { pixelSize: 12; bold: true; family: "Inter" } 
                                }
                                MouseArea {
                                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (setupWizardLayout.currentIndex === 0) {
                                            if (vaultSizeSlider.value < vaultSizeSlider.to) {
                                                setupWizardLayout.currentIndex = 1
                                            } else {
                                                setupWizardLayout.currentIndex = 2
                                            }
                                        } else if (setupWizardLayout.currentIndex === 1) {
                                            setupWizardLayout.currentIndex = 2
                                        } else {
                                            var force = vaultController.isSelectedDeviceVault || forceCheckbox.checked
                                            vaultController.setupVault(
                                                vaultController.selectedDevice, 
                                                force, 
                                                parseInt(ownerIdInput.text), 
                                                parseInt(vaultSizeSlider.value),
                                                vaultNameInput.text,
                                                normalNameInput.text,
                                                normalFsCombo.currentText
                                            )
                                            setupWarningDialog.close()
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

        function handleBack() {
            if (setupWizardLayout.currentIndex === 0) {
                setupWarningDialog.close()
            } else if (setupWizardLayout.currentIndex === 1) {
                setupWizardLayout.currentIndex = 0
            } else {
                if (vaultSizeSlider.value < vaultSizeSlider.to) {
                    setupWizardLayout.currentIndex = 1
                } else {
                    setupWizardLayout.currentIndex = 0
                }
            }
        }

        onClosed: {
            forceCheckbox.checked = false
            setupResultText.visible = false
            setupWizardLayout.currentIndex = 0
            vaultNameInput.text = "My Vault"
            ownerIdInput.text = "1"
        }
    }
}
