import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DigitalVault

Column {
    id: root
    property alias normalName: normalNameField.text
    property alias normalNameInput: normalNameField.input
    property alias filesystemCombo: fsCombo
    spacing: 16
    width: parent ? parent.width : implicitWidth

    RowLayout {
        width: parent.width
        spacing: 16
        WizardTextField {
            id: normalNameField
            Layout.fillWidth: true
            label: "Normal partition name"
            text: "Normal"
        }
        Column {
            Layout.preferredWidth: 140
            spacing: 6
            WizardFieldLabel { label: "Filesystem" }
            ComboBox {
                id: fsCombo
                width: parent.width
                model: ["exfat", "vfat", "ext4"]
                currentIndex: 0
            }
        }
    }
}
