import QtQuick
import DigitalVault

Row {
    id: root
    width: parent ? parent.width : 400
    height: 40
    spacing: 12

    property string title: ""
    property string subtitle: ""
    property bool showBack: true

    signal backRequested()

    Rectangle {
        width: backLbl.implicitWidth + 20
        height: 32
        radius: 16
        color: backMouse.containsMouse ? Theme.bgHover : Theme.bgOverlay
        border.color: Theme.border
        visible: root.showBack

        Text {
            id: backLbl
            anchors.centerIn: parent
            text: "← Back"
            color: Theme.textPrimary
            font { pixelSize: 12; family: Theme.fontFamily }
        }

        MouseArea {
            id: backMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.backRequested()
        }
    }

    Column {
        width: Math.max(0, root.width - (root.showBack ? backLbl.parent.width + 12 : 0))
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2

        Text {
            width: parent.width
            text: root.title
            color: Theme.textPrimary
            font { pixelSize: root.showBack ? 18 : 26; bold: true; family: Theme.fontFamily }
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            visible: root.subtitle !== ""
            text: root.subtitle
            color: Theme.textMuted
            font { pixelSize: 11; family: Theme.fontFamily }
            elide: Text.ElideRight
        }
    }
}
