import QtQuick
import QtQuick.Controls
import DigitalVault

Rectangle {
    id: root
    property string text: ""
    property string variant: "secondary" // secondary | danger | primary
    signal clicked()

    clip: true
    implicitWidth: Math.max(88, label.implicitWidth + 28)
    implicitHeight: 36
    radius: 8
    color: {
        if (variant === "danger")
            return btnMouse.containsMouse ? "#e74c3c" : Theme.danger
        if (variant === "primary")
            return btnMouse.containsMouse ? Theme.accent : Theme.accent
        return btnMouse.containsMouse ? Theme.bgOverlay : "transparent"
    }
    border.color: variant === "danger" ? Theme.danger : Theme.border
    border.width: 1

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: variant === "secondary" ? Theme.textPrimary : "white"
        font { pixelSize: 13; family: "Inter"; bold: true }
    }

    MouseArea {
        id: btnMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
