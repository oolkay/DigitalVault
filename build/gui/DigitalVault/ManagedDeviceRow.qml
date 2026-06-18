import QtQuick
import DigitalVault

Rectangle {
    id: root
    width: parent ? parent.width : 400
    height: 82
    radius: Theme.radius
    color: rowMouse.containsMouse ? Theme.bgHover : Theme.bgCard
    border.color: Theme.border
    border.width: 1

    property var deviceData: ({})
    readonly property bool canShowUnlock: (deviceData.statusLabel || "") === "Locked"
    readonly property bool canShowSetup: (deviceData.statusLabel || "") === "Container"
    readonly property real actionsWidth: actionRow.visible ? actionRow.implicitWidth + 8 : 0

    signal rowClicked()
    signal deleteClicked()
    signal unlockClicked()
    signal setupClicked()

    MouseArea {
        id: rowMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.rowClicked()
    }

    Row {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        Column {
            width: Math.max(120, parent.width - statusBadge.width - chevron.width - root.actionsWidth - 24)
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4

            Text {
                width: parent.width
                text: deviceData.displayName || deviceData.name || deviceData.path || "Vault"
                color: Theme.textPrimary
                font { pixelSize: 14; bold: true; family: Theme.fontFamily }
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                text: deviceData.path || ""
                color: Theme.textSecondary
                font { pixelSize: 11; family: Theme.fontFamily }
                elide: Text.ElideRight
            }
        }

        Row {
            id: actionRow
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            visible: deleteBtn.visible || unlockBtn.visible || setupBtn.visible

            Rectangle {
                id: deleteBtn
                visible: !!deviceData.canDelete
                width: deleteText.implicitWidth + 18
                height: 30
                radius: 8
                color: deleteMouse.containsMouse ? Theme.dangerBg : "transparent"
                border.color: Theme.danger
                border.width: 1
                Text {
                    id: deleteText
                    anchors.centerIn: parent
                    text: "Delete"
                    color: Theme.danger
                    font { pixelSize: 11; bold: true; family: Theme.fontFamily }
                }
                MouseArea {
                    id: deleteMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.deleteClicked()
                }
            }

            Rectangle {
                id: unlockBtn
                visible: root.canShowUnlock
                width: unlockText.implicitWidth + 18
                height: 30
                radius: 8
                color: unlockMouse.containsMouse ? Theme.accentHover : Theme.accent
                Text {
                    id: unlockText
                    anchors.centerIn: parent
                    text: "Unlock"
                    color: "white"
                    font { pixelSize: 11; bold: true; family: Theme.fontFamily }
                }
                MouseArea {
                    id: unlockMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.unlockClicked()
                }
            }

            Rectangle {
                id: setupBtn
                visible: root.canShowSetup
                width: setupText.implicitWidth + 18
                height: 30
                radius: 8
                color: setupMouse.containsMouse ? Qt.darker(Theme.warning, 1.15) : Theme.warning
                border.color: Theme.warning
                border.width: 1
                Text {
                    id: setupText
                    anchors.centerIn: parent
                    text: "Setup encryption"
                    color: "white"
                    font { pixelSize: 11; bold: true; family: Theme.fontFamily }
                }
                MouseArea {
                    id: setupMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.setupClicked()
                }
            }
        }

        Rectangle {
            id: statusBadge
            anchors.verticalCenter: parent.verticalCenter
            height: 22
            width: statusLabel.implicitWidth + 16
            radius: 11
            color: {
                const s = deviceData.statusLabel || ""
                if (s === "Unlocked") return Theme.successBg
                if (s === "Unavailable") return Theme.dangerBg
                return Theme.bgOverlay
            }
            Text {
                id: statusLabel
                anchors.centerIn: parent
                text: deviceData.statusLabel || "Locked"
                color: {
                    const s = deviceData.statusLabel || ""
                    if (s === "Unlocked") return Theme.success
                    if (s === "Unavailable") return Theme.danger
                    return Theme.textSecondary
                }
                font { pixelSize: 10; bold: true; family: Theme.fontFamily }
            }
        }

        Text {
            id: chevron
            anchors.verticalCenter: parent.verticalCenter
            text: "→"
            color: Theme.textMuted
            font { pixelSize: 16; family: Theme.fontFamily }
        }
    }
}
