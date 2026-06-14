import QtQuick
import DigitalVault

Rectangle {
    id: root
    width: parent ? parent.width : implicitWidth
    height: summaryCol.implicitHeight + 24
    color: Qt.rgba(1, 1, 1, 0.03)
    radius: 10
    border.color: Theme.border
    border.width: 1

    default property alias content: summaryCol.children

    Column {
        id: summaryCol
        anchors { fill: parent; margins: 12 }
        spacing: 8
    }
}
