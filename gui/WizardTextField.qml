import QtQuick
import DigitalVault

Column {
    id: root
    required property string label
    property alias text: fieldInput.text
    property alias input: fieldInput
    property int fieldHeight: 38
    property bool readOnly: false
    spacing: 6
    width: parent ? parent.width : implicitWidth

    WizardFieldLabel { label: root.label }

    Rectangle {
        width: parent.width
        height: root.fieldHeight
        radius: 8
        color: root.readOnly ? Theme.bgOverlay : Theme.bgInput
        border.color: Theme.border
        border.width: 1

        Text {
            visible: root.readOnly
            anchors { fill: parent; leftMargin: 12; rightMargin: 12; verticalCenter: parent.verticalCenter }
            text: fieldInput.text
            color: Theme.textSecondary
            font { pixelSize: 11; family: "Inter" }
            elide: Text.ElideMiddle
            verticalAlignment: Text.AlignVCenter
        }

        TextInput {
            id: fieldInput
            visible: !root.readOnly
            anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
            verticalAlignment: TextInput.AlignVCenter
            color: Theme.textPrimary
            font { pixelSize: 14; family: "Inter" }
        }
    }
}
