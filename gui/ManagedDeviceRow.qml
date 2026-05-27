import QtQuick
import DigitalVault

Rectangle {
    id: root
    width: parent ? parent.width : 400
    height: 72
    radius: Theme.radius
    color: rowMouse.containsMouse ? Theme.bgHover : Theme.bgCard
    border.color: Theme.border
    border.width: 1

    property var deviceData: ({})

    signal rowClicked()

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
            width: Math.max(120, parent.width - statusBadge.width - chevron.width - 24)
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

        Text {
            id: chevron
            anchors.verticalCenter: parent.verticalCenter
            text: "→"
            color: Theme.textMuted
            font { pixelSize: 16; family: "Inter" }
        }
    }
}
