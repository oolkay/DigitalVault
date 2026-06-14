import QtQuick
import QtQuick.Controls
import DigitalVault

Rectangle {
    id: root
    property string text: ""
    property string variant: "secondary" // secondary | danger | primary
    signal clicked()

    clip: true
    implicitWidth: Math.max(104, label.implicitWidth + 32)
    implicitHeight: 42
    radius: 8
    opacity: enabled ? 1.0 : 0.45
    color: {
        if (variant === "danger")
            return btnMouse.containsMouse ? "#F06A6A" : Theme.danger
        if (variant === "primary")
            return btnMouse.containsMouse ? Theme.accentHover : Theme.accent
        return btnMouse.containsMouse ? Theme.bgOverlay : "transparent"
    }
    border.color: variant === "danger" ? Theme.danger : Theme.border
    border.width: 1

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: variant === "secondary" ? Theme.textPrimary : "white"
        font { pixelSize: 14; family: Theme.fontFamily; bold: true }
    }

    MouseArea {
        id: btnMouse
        anchors.fill: parent
        enabled: root.enabled
        hoverEnabled: true
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.clicked()
    }
}
