import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DigitalVault

Item {
    id: deviceGuidePage

    // ── Live Device Scanner ────────────────────────────────────────────────
    property var deviceList: []
    property int scanningIndex: -1
    property string scanResult: ""

    function refreshDevices() {
        deviceList = vaultController.getStorageDevices()
        scanResult = ""
        scanningIndex = -1
    }

    Component.onCompleted: refreshDevices()

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

            // ── Header ──────────────────────────────────────────────────
            Row {
                width: parent.width
                spacing: 16
                Column {
                    spacing: 6
                    Text {
                        text: "Device Guide"
                        color: Theme.textPrimary
                        font { pixelSize: 26; bold: true; family: "Inter" }
                        Behavior on color { ColorAnimation { duration: 250 } }
                    }
                    Text {
                        text: "Inspect connected devices before formatting. Understand what each device contains."
                        color: Theme.textSecondary
                        font { pixelSize: 13; family: "Inter" }
                        Behavior on color { ColorAnimation { duration: 250 } }
                    }
                }
                Item { Layout.fillWidth: true; width: 1 }
            }

            // ── Status Legend ────────────────────────────────────────────
            Rectangle {
                width: parent.width
                height: legendCol.implicitHeight + 40
                radius: Theme.radius
                color: Theme.bgCard
                border.color: Theme.border
                border.width: 1
                Behavior on color { ColorAnimation { duration: 250 } }
                Behavior on border.color { ColorAnimation { duration: 250 } }

                // Top accent
                Rectangle {
                    anchors { top: parent.top; left: parent.left; right: parent.right }
                    height: 3; radius: parent.radius
                    color: Theme.accent
                }

                Column {
                    id: legendCol
                    anchors { fill: parent; margins: 20; topMargin: 24 }
                    spacing: 16

                    Text {
                        text: "📖  How to Read Device Status"
                        color: Theme.textPrimary
                        font { pixelSize: 15; bold: true; family: "Inter" }
                        Behavior on color { ColorAnimation { duration: 250 } }
                    }

                    Grid {
                        columns: 2
                        columnSpacing: 32
                        rowSpacing: 12
                        width: parent.width

                        Repeater {
                            model: [
                                { icon: "🔐", badge: "LUKS VAULT",   color: "#4E6BF5", bg: "#4E6BF520",
                                  title: "DigitalVault Encrypted",
                                  desc: "This device already has a LUKS2 container created by DigitalVault.\nUse 'Open Vault' to unlock it — do NOT run Setup again." },
                                { icon: "💾", badge: "HAS DATA",     color: "#F5A623", bg: "#F5A62320",
                                  title: "Existing Filesystem",
                                  desc: "Device contains data (FAT32, NTFS, ext4, etc.).\nFormatting will permanently destroy all existing files." },
                                { icon: "⬜", badge: "EMPTY",        color: "#2ECC88", bg: "#2ECC8820",
                                  title: "Empty / Unformatted",
                                  desc: "Device has no recognizable filesystem.\nSafe to run Setup and create a new vault." },
                                { icon: "🔁", badge: "LOOP DEVICE",  color: "#7A5AF8", bg: "#7A5AF820",
                                  title: "Virtual Loop Device",
                                  desc: "A file mounted as a virtual disk (e.g. test_vault.img).\nUsed for testing without a physical USB drive." }
                            ]
                            delegate: Rectangle {
                                width: (legendCol.width - 32) / 2
                                height: legendItem.implicitHeight + 24
                                radius: 12
                                color: modelData.bg
                                border.color: modelData.color
                                border.width: 1

                                Column {
                                    id: legendItem
                                    anchors { fill: parent; margins: 16 }
                                    spacing: 8

                                    Row {
                                        spacing: 10
                                        Text { text: modelData.icon; font.pixelSize: 20; anchors.verticalCenter: parent.verticalCenter }
                                        Rectangle {
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: badgeTxt.implicitWidth + 16; height: 22; radius: 11
                                            color: modelData.color
                                            Text {
                                                id: badgeTxt
                                                anchors.centerIn: parent
                                                text: modelData.badge
                                                color: "white"
                                                font { pixelSize: 10; bold: true; family: "Inter" }
                                            }
                                        }
                                    }

                                    Text {
                                        text: modelData.title
                                        color: Theme.textPrimary
                                        font { pixelSize: 13; bold: true; family: "Inter" }
                                        Behavior on color { ColorAnimation { duration: 250 } }
                                    }

                                    Text {
                                        text: modelData.desc
                                        color: Theme.textSecondary
                                        font { pixelSize: 12; family: "Inter" }
                                        width: parent.width
                                        wrapMode: Text.Wrap
                                        Behavior on color { ColorAnimation { duration: 250 } }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ── Live Device Scanner ──────────────────────────────────────
            Rectangle {
                width: parent.width
                height: scannerCol.implicitHeight + 40
                radius: Theme.radius
                color: Theme.bgCard
                border.color: Theme.border
                border.width: 1
                Behavior on color { ColorAnimation { duration: 250 } }
                Behavior on border.color { ColorAnimation { duration: 250 } }

                Column {
                    id: scannerCol
                    anchors { fill: parent; margins: 20 }
                    spacing: 16

                    // Header row
                    Item {
                        width: parent.width
                        height: 36

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "🔍  Live Device Scanner"
                            color: Theme.textPrimary
                            font { pixelSize: 15; bold: true; family: "Inter" }
                            Behavior on color { ColorAnimation { duration: 250 } }
                        }

                        Rectangle {
                            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                            width: refreshLabel.implicitWidth + 24; height: 32; radius: 16
                            color: refreshMouse.containsMouse ? Theme.accentHover : Theme.accent
                            Behavior on color { ColorAnimation { duration: 150 } }
                            scale: refreshMouse.containsMouse ? 1.04 : 1.0
                            Behavior on scale { NumberAnimation { duration: 150 } }

                            Text {
                                id: refreshLabel
                                anchors.centerIn: parent
                                text: "↻  Refresh"
                                color: "white"
                                font { pixelSize: 12; bold: true; family: "Inter" }
                            }
                            MouseArea {
                                id: refreshMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: refreshDevices()
                            }
                        }
                    }

                    // Device list
                    Column {
                        width: parent.width
                        spacing: 10

                        Repeater {
                            model: deviceGuidePage.deviceList

                            delegate: Rectangle {
                                id: deviceRow
                                width: parent.width
                                height: 72
                                radius: 12
                                color: devRowMouse.containsMouse ? Theme.bgHover : Theme.bgOverlay
                                border.color: Theme.border
                                border.width: 1
                                Behavior on color { ColorAnimation { duration: 150 } }

                                property string devType: {
                                    var path = modelData.path
                                    if (path.indexOf("loop") >= 0) return "loop"
                                    return "disk"
                                }

                                Item {
                                    anchors { fill: parent; leftMargin: 16; rightMargin: 16 }

                                    // Icon
                                    Rectangle {
                                        id: devIconRect
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 44; height: 44; radius: 10
                                        color: deviceRow.devType === "loop" ? "#7A5AF820" : "#F5A62320"
                                        Text {
                                            anchors.centerIn: parent
                                            text: deviceRow.devType === "loop" ? "🔁" : "💾"
                                            font.pixelSize: 20
                                        }
                                    }

                                    // Name + path
                                    Column {
                                        anchors {
                                            left: devIconRect.right; leftMargin: 14
                                            verticalCenter: parent.verticalCenter
                                        }
                                        spacing: 4
                                        Text {
                                            text: modelData.name
                                            color: Theme.textPrimary
                                            font { pixelSize: 13; bold: true; family: "Inter" }
                                            Behavior on color { ColorAnimation { duration: 250 } }
                                        }
                                        Text {
                                            text: modelData.path
                                            color: Theme.textMuted
                                            font { pixelSize: 11; family: "Courier New" }
                                            Behavior on color { ColorAnimation { duration: 250 } }
                                        }
                                    }

                                    // Scan button (right side)
                                    Rectangle {
                                        id: scanBtn
                                        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                                        width: scanBtnLabel.implicitWidth + 24; height: 30; radius: 15
                                        color: scanBtnMouse.containsMouse ? Theme.bgHover : "transparent"
                                        border.color: Theme.border
                                        border.width: 1
                                        Behavior on color { ColorAnimation { duration: 150 } }

                                        Text {
                                            id: scanBtnLabel
                                            anchors.centerIn: parent
                                            text: "Scan Device"
                                            color: Theme.textSecondary
                                            font { pixelSize: 11; bold: true; family: "Inter" }
                                            Behavior on color { ColorAnimation { duration: 250 } }
                                        }
                                        MouseArea {
                                            id: scanBtnMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                deviceGuidePage.scanningIndex = index
                                                scanResultDrawer.targetDevice = modelData.path
                                                scanResultDrawer.targetName   = modelData.name
                                                scanResultDrawer.open()
                                            }
                                        }
                                    }
                                }

                                MouseArea {
                                    id: devRowMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.ArrowCursor
                                    // Prevent consuming clicks meant for scanBtnMouse
                                    onClicked: {}
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
                            visible: deviceGuidePage.deviceList.length === 0
                            Text {
                                anchors.centerIn: parent
                                text: "No devices found. Plug in a USB drive or create a loop device."
                                color: Theme.textMuted
                                font { pixelSize: 13; family: "Inter" }
                                Behavior on color { ColorAnimation { duration: 250 } }
                            }
                        }
                    }
                }
            }

            // ── Terminal Commands Reference ──────────────────────────────
            Rectangle {
                width: parent.width
                height: cmdCol.implicitHeight + 40
                radius: Theme.radius
                color: Theme.bgCard
                border.color: Theme.border
                border.width: 1
                Behavior on color { ColorAnimation { duration: 250 } }
                Behavior on border.color { ColorAnimation { duration: 250 } }

                Column {
                    id: cmdCol
                    anchors { fill: parent; margins: 20 }
                    spacing: 14

                    Text {
                        text: "⌨  Terminal Quick Reference"
                        color: Theme.textPrimary
                        font { pixelSize: 15; bold: true; family: "Inter" }
                        Behavior on color { ColorAnimation { duration: 250 } }
                    }

                    Repeater {
                        model: [
                            { cmd: "lsblk -f /dev/sdX1",
                              desc: "Show filesystem type, label, UUID of a partition. Best first check." },
                            { cmd: "sudo cryptsetup isLuks /dev/sdX1 && echo LUKS",
                              desc: "Exits 0 if device is a LUKS container. Safe, read-only check." },
                            { cmd: "sudo blkid /dev/sdX1",
                              desc: "Returns TYPE and UUID if any filesystem is detected." },
                            { cmd: "sudo hexdump -C /dev/sdX1 | head -5",
                              desc: "Raw bytes — random-looking output means encrypted or empty." },
                            { cmd: "dd if=/dev/zero of=~/test.img bs=1M count=256",
                              desc: "Create a 256 MiB virtual disk image for safe testing." },
                            { cmd: "sudo losetup -fP ~/test.img && lsblk | grep loop",
                              desc: "Attach the image as a loop device, then verify it appeared." }
                        ]
                        delegate: Rectangle {
                            width: parent.width
                            height: cmdContent.implicitHeight + 20
                            radius: 10
                            color: Theme.bgOverlay
                            border.color: Theme.border
                            border.width: 1

                            Column {
                                id: cmdContent
                                anchors { fill: parent; margins: 14 }
                                spacing: 6

                                Rectangle {
                                    width: parent.width
                                    height: cmdText.implicitHeight + 12
                                    radius: 6
                                    color: Theme.bgBase

                                    Text {
                                        id: cmdText
                                        anchors { fill: parent; margins: 8 }
                                        text: modelData.cmd
                                        color: "#A5D8FF"
                                        font { pixelSize: 12; family: "Courier New" }
                                        wrapMode: Text.Wrap
                                        Behavior on color { ColorAnimation { duration: 250 } }
                                    }
                                }

                                Text {
                                    text: modelData.desc
                                    color: Theme.textSecondary
                                    font { pixelSize: 12; family: "Inter" }
                                    width: parent.width
                                    wrapMode: Text.Wrap
                                    Behavior on color { ColorAnimation { duration: 250 } }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ── Scan Result Popup ──────────────────────────────────────────────────
    Drawer {
        id: scanResultDrawer
        width: Math.min(parent.width * 0.45, 420)
        height: parent.height
        edge: Qt.RightEdge
        dim: true
        modal: true

        property string targetDevice: ""
        property string targetName: ""
        property string fsType: ""
        property bool isLuks: false
        property bool scanning: false

        background: Rectangle {
            color: Theme.bgSurface
            Behavior on color { ColorAnimation { duration: 250 } }
        }

        onOpened: {
            scanning = true
            fsType = ""
            isLuks = false
            scanTimer.start()
        }

        // Simulate async scan via timer to keep UI responsive
        Timer {
            id: scanTimer
            interval: 100
            repeat: false
            onTriggered: {
                // These are reflected from vault_manager check_device_status via lsblk
                var devs = vaultController.getStorageDevices()
                scanResultDrawer.scanning = false
            }
        }

        Column {
            anchors { fill: parent; margins: 28 }
            spacing: 20

            // Handle
            Rectangle {
                width: 40; height: 4; radius: 2
                color: Theme.border
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                text: "Device Inspection"
                color: Theme.textPrimary
                font { pixelSize: 18; bold: true; family: "Inter" }
                Behavior on color { ColorAnimation { duration: 250 } }
            }

            // Device name
            Rectangle {
                width: parent.width
                height: devInfoCol.implicitHeight + 20
                radius: 12
                color: Theme.bgOverlay
                border.color: Theme.border
                border.width: 1

                Column {
                    id: devInfoCol
                    anchors { fill: parent; margins: 16 }
                    spacing: 6
                    Text {
                        text: "Device"
                        color: Theme.textMuted
                        font { pixelSize: 11; bold: true; family: "Inter" }
                        Behavior on color { ColorAnimation { duration: 250 } }
                    }
                    Text {
                        text: scanResultDrawer.targetDevice
                        color: Theme.accent
                        font { pixelSize: 14; bold: true; family: "Courier New" }
                    }
                    Text {
                        text: scanResultDrawer.targetName
                        color: Theme.textSecondary
                        font { pixelSize: 12; family: "Inter" }
                        width: parent.width
                        elide: Text.ElideRight
                        Behavior on color { ColorAnimation { duration: 250 } }
                    }
                }
            }

            // Steps info
            Column {
                width: parent.width
                spacing: 12

                Text {
                    text: "Run these commands in terminal to inspect:"
                    color: Theme.textSecondary
                    font { pixelSize: 13; family: "Inter" }
                    Behavior on color { ColorAnimation { duration: 250 } }
                }

                Repeater {
                    model: [
                        { step: "1",  label: "Quick overview",
                          cmd: "lsblk -f " + scanResultDrawer.targetDevice },
                        { step: "2", label: "LUKS check",
                          cmd: "sudo cryptsetup isLuks " + scanResultDrawer.targetDevice + " && echo 'LUKS vault found'" },
                        { step: "3", label: "Filesystem type",
                          cmd: "sudo blkid " + scanResultDrawer.targetDevice },
                        { step: "4", label: "Raw bytes (encrypted?)",
                          cmd: "sudo hexdump -C " + scanResultDrawer.targetDevice + " | head -4" }
                    ]
                    delegate: Rectangle {
                        width: parent.width
                        height: stepCol.implicitHeight + 20
                        radius: 10
                        color: Theme.bgCard
                        border.color: Theme.border
                        border.width: 1
                        Behavior on color { ColorAnimation { duration: 250 } }

                        Column {
                            id: stepCol
                            anchors { fill: parent; margins: 14 }
                            spacing: 8

                            Row {
                                spacing: 8
                                Rectangle {
                                    width: 22; height: 22; radius: 11
                                    color: Theme.accent
                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.step
                                        color: "white"
                                        font { pixelSize: 11; bold: true }
                                    }
                                }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData.label
                                    color: Theme.textSecondary
                                    font { pixelSize: 12; bold: true; family: "Inter" }
                                    Behavior on color { ColorAnimation { duration: 250 } }
                                }
                            }

                            Rectangle {
                                width: parent.width
                                height: cmdLabel.implicitHeight + 12
                                radius: 6
                                color: Theme.bgBase

                                Text {
                                    id: cmdLabel
                                    anchors { fill: parent; margins: 8 }
                                    text: modelData.cmd
                                    color: "#A5D8FF"
                                    font { pixelSize: 11; family: "Courier New" }
                                    wrapMode: Text.Wrap
                                }
                            }
                        }
                    }
                }
            }

            // Close button
            Rectangle {
                width: parent.width; height: 40; radius: 20
                color: closeMouse.containsMouse ? Theme.bgHover : Theme.bgOverlay
                border.color: Theme.border; border.width: 1
                Behavior on color { ColorAnimation { duration: 150 } }
                Text {
                    anchors.centerIn: parent
                    text: "Close"
                    color: Theme.textSecondary
                    font { pixelSize: 13; bold: true; family: "Inter" }
                    Behavior on color { ColorAnimation { duration: 250 } }
                }
                MouseArea {
                    id: closeMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: scanResultDrawer.close()
                }
            }
        }
    }
}
