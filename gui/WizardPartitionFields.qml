import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DigitalVault

Column {
    id: root
    property alias normalName: normalNameField.text
    property alias normalNameInput: normalNameField.input
    property alias filesystemCombo: fsPicker
    spacing: 16
    width: parent ? parent.width : implicitWidth

    RowLayout {
        width: parent.width
        spacing: 16
        WizardTextField {
            id: normalNameField
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            label: "Normal partition name"
            text: "Normal"
        }
        Column {
            Layout.preferredWidth: 180
            Layout.alignment: Qt.AlignTop
            spacing: 6
            WizardFieldLabel { label: "Filesystem" }
            Column {
                id: fsPicker
                width: parent.width
                spacing: 4
                property var model: vaultController.supportedFilesystems()
                property int currentIndex: 0
                readonly property string currentText: model.length > currentIndex ? model[currentIndex] : "exfat"
                property bool expanded: false

                Rectangle {
                    width: parent.width
                    height: 38
                    radius: 8
                    color: Theme.bgInput
                    border.color: Theme.border
                    border.width: 1

                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 10
                        spacing: 8
                        Text {
                            width: Math.max(0, parent.width - arrow.width - parent.spacing)
                            anchors.verticalCenter: parent.verticalCenter
                            text: fsPicker.currentText
                            color: Theme.textPrimary
                            font { pixelSize: 13; family: Theme.fontFamily }
                            elide: Text.ElideRight
                        }
                        Text {
                            id: arrow
                            anchors.verticalCenter: parent.verticalCenter
                            text: fsPicker.expanded ? "▲" : "▼"
                            color: Theme.textSecondary
                            font { pixelSize: 10; family: Theme.fontFamily }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: fsPicker.expanded = !fsPicker.expanded
                    }
                }

                Column {
                    width: parent.width
                    visible: fsPicker.expanded
                    spacing: 2
                    Repeater {
                        model: fsPicker.model
                        delegate: Rectangle {
                            width: fsPicker.width
                            height: 34
                            radius: 7
                            color: fsChoiceMouse.containsMouse || index === fsPicker.currentIndex
                                   ? Theme.accentGlow : Theme.bgCard
                            border.color: Theme.border
                            border.width: 1
                            Text {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                verticalAlignment: Text.AlignVCenter
                                text: modelData
                                color: index === fsPicker.currentIndex ? Theme.accent : Theme.textPrimary
                                font { pixelSize: 13; bold: index === fsPicker.currentIndex; family: Theme.fontFamily }
                            }
                            MouseArea {
                                id: fsChoiceMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    fsPicker.currentIndex = index
                                    fsPicker.expanded = false
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
