import QtQuick
import DigitalVault

Rectangle {
    id: root
    width: parent ? parent.width : 400
    height: 76
    radius: Theme.radius
    color: Theme.bgCard
    border.color: Theme.border
    border.width: 1

    property var deviceData: ({})
    signal setupRequested()

    Row {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        Column {
            width: parent.width - setupBtn.width - 24
            anchors.verticalCenter: parent.verticalCenter
            spacing: 3

            Text {
                width: parent.width
                elide: Text.ElideRight
                text: deviceData.name || deviceData.path || "Storage"
                color: Theme.textPrimary
                font { pixelSize: 13; bold: true; family: Theme.fontFamily }
            }
            Text {
                width: parent.width
                elide: Text.ElideRight
                text: (deviceData.description || "") + (deviceData.path ? " · " + deviceData.path : "")
                color: Theme.textSecondary
                font { pixelSize: 11; family: Theme.fontFamily }
            }
            Text {
                width: parent.width
                elide: Text.ElideRight
                text: "Setup will ask for confirmation before erasing or formatting."
                color: Theme.warning
                font { pixelSize: 10; family: Theme.fontFamily }
            }
        }

        Rectangle {
            id: setupBtn
            anchors.verticalCenter: parent.verticalCenter
            width: setupLbl.implicitWidth + 24
            height: 32
            radius: Theme.radiusSm
            color: setupMouse.containsMouse ? Theme.accentHover : Theme.accent
            Text {
                id: setupLbl
                anchors.centerIn: parent
                text: "Set up encryption"
                color: "white"
                font { pixelSize: 11; bold: true; family: Theme.fontFamily }
            }
            MouseArea {
                id: setupMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.setupRequested()
            }
        }
    }
}
