import QtQuick
import DigitalVault

Flow {
    id: root
    width: parent ? parent.width : 400
    spacing: 8

    property var actions: []
    signal actionTriggered(string actionId)

    Repeater {
        model: root.actions
        delegate: Rectangle {
            required property var modelData
            width: actionLbl.implicitWidth + 24
            height: 32
            radius: 16
            color: actionMouse.containsMouse
                   ? (modelData.dangerous ? Theme.dangerBg : Theme.bgHover)
                   : Theme.bgOverlay
            border.color: modelData.dangerous ? Theme.danger : Theme.border
            border.width: 1

            Text {
                id: actionLbl
                anchors.centerIn: parent
                text: modelData.label || modelData.id || ""
                color: modelData.dangerous ? Theme.danger : Theme.textPrimary
                font { pixelSize: 11; family: "Inter" }
            }

            MouseArea {
                id: actionMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.actionTriggered(modelData.id || "")
            }
        }
    }
}
