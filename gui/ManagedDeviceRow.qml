import QtQuick
import QtQuick.Controls
import DigitalVault

Rectangle {
    id: root
    width: parent ? parent.width : 400
    height: 72
    radius: Theme.radius
    color: isSelected ? Theme.bgHover : Theme.bgCard
    border.color: isSelected ? Theme.accent : Theme.border
    border.width: 1

    property var deviceData: ({})
    property bool isSelected: false

    signal rowClicked()
    signal renameRequested()
    signal deleteRequested()

    MouseArea {
        anchors.fill: parent
        z: -1
        onClicked: root.rowClicked()
    }

    Row {
        id: row
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        Column {
            width: Math.max(120, parent.width - statusBadge.width - actionRow.width - 24)
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4

            Text {
                width: parent.width
                text: deviceData.displayName || deviceData.name || deviceData.path || "Vault"
                color: Theme.textPrimary
                font { pixelSize: 14; bold: true; family: "Inter" }
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                text: deviceData.path || ""
                color: Theme.textSecondary
                font { pixelSize: 11; family: "Inter" }
                elide: Text.ElideRight
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
                color: Theme.textSecondary
                font { pixelSize: 10; family: "Inter" }
            }
        }

        Row {
            id: actionRow
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            Rectangle {
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

            Rectangle {
                width: delLbl.implicitWidth + 20
                height: 30
                radius: 15
                color: delMouse.containsMouse ? Theme.dangerBg : Theme.dangerBg
                border.color: Theme.danger
                Text {
                    id: delLbl
                    anchors.centerIn: parent
                    text: "Delete vault"
                    color: Theme.danger
                    font { pixelSize: 11; bold: true; family: "Inter" }
                }
                MouseArea {
                    id: delMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.deleteRequested()
                }
            }
        }
    }
}
