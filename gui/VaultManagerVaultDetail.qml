import QtQuick
import QtQuick.Controls
import DigitalVault

Item {
    id: root
    width: parent ? parent.width : 0
    height: parent ? parent.height : 0

    property string devicePath: ""
    property var layoutData: ({})
    property string selectedPartitionPath: ""

    signal backRequested()
    signal partitionPathChanged(string path)
    signal renameRequested()
    signal deleteRequested()
    signal setupEncryptionRequested()

    function deviceEntry() {
        const devices = vaultController.managedDevices
        for (let i = 0; i < devices.length; ++i) {
            if (devices[i].path === devicePath)
                return devices[i]
        }
        return {}
    }

    readonly property var deviceData: deviceEntry()

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        Column {
            width: parent.width - 24
            spacing: 20
            leftPadding: 12
            rightPadding: 12
            topPadding: 4
            bottomPadding: 24

            VaultManagerNavBar {
                width: parent.width
                title: deviceData.displayName || deviceData.name || devicePath || "Vault"
                subtitle: devicePath
                onBackRequested: root.backRequested()
            }

            Rectangle {
                width: parent.width
                radius: Theme.radius
                color: Theme.bgCard
                border.color: Theme.border
                border.width: 1
                implicitHeight: summaryCol.implicitHeight + 28

                Column {
                    id: summaryCol
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    Item {
                        width: parent.width
                        height: 30

                        Row {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 12

                            Rectangle {
                                height: 24
                                width: summaryStatus.implicitWidth + 16
                                radius: 12
                                color: {
                                    const s = deviceData.statusLabel || ""
                                    if (s === "Unlocked") return Theme.successBg
                                    if (s === "Unavailable") return Theme.dangerBg
                                    return Theme.bgOverlay
                                }
                                Text {
                                    id: summaryStatus
                                    anchors.centerIn: parent
                                    text: deviceData.statusLabel || "Locked"
                                    color: Theme.textSecondary
                                    font { pixelSize: 10; family: "Inter" }
                                }
                            }

                            Text {
                                id: dashLink
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

                        Rectangle {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            width: renameLbl.implicitWidth + 20
                            height: 30
                            radius: 15
                            color: renameMouse.containsMouse ? Theme.bgHover : Theme.bgOverlay
                            border.color: Theme.border

                            Text {
                                id: renameLbl
                                anchors.centerIn: parent
                                text: "Rename"
                                color: Theme.textPrimary
                                font { pixelSize: 11; family: "Inter" }
                            }

                            MouseArea {
                                id: renameMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.renameRequested()
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: deviceData.description || ""
                        visible: text !== ""
                        color: Theme.textSecondary
                        font { pixelSize: 11; family: "Inter" }
                    }
                }
            }

            Rectangle {
                width: parent.width
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
                        layoutData: root.layoutData
                        selectedPartitionPath: root.selectedPartitionPath
                        onPartitionSelected: function(path) {
                            root.partitionPathChanged(path)
                        }
                    }

                    PartitionLayoutTable {
                        width: parent.width
                        visible: (root.layoutData.partitions || []).length > 0
                        partitions: root.layoutData.partitions || []
                        selectedPartitionPath: root.selectedPartitionPath
                        onRowClicked: function(path) {
                            root.partitionPathChanged(path)
                        }
                    }

                    StorageActionPanel {
                        width: parent.width
                        actions: vaultController.availablePartitionActions(
                            root.devicePath, root.selectedPartitionPath)
                        onActionTriggered: function(actionId) {
                            if (actionId === "setup_encryption") {
                                root.setupEncryptionRequested()
                                return
                            }
                            if (actionId === "delete_vault") {
                                root.deleteRequested()
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
                            onClicked: root.deleteRequested()
                        }
                    }
                }
            }
        }
    }
}
