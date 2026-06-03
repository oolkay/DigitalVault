import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DigitalVault

Column {
    id: root
    property string label: "Size"
    property alias slider: sizeSlider
    property alias from: sizeSlider.from
    property alias to: sizeSlider.to
    property alias value: sizeSlider.value
    property alias stepSize: sizeSlider.stepSize
    property string hintText: ""
    property bool updatingFromSlider: false
    property bool updatingFromText: false
    property bool invalidInput: false
    readonly property int gbThresholdMB: 1024
    spacing: 12
    width: parent ? parent.width : implicitWidth

    function formatAmount(mb) {
        if (mb >= gbThresholdMB)
            return (mb / gbThresholdMB).toFixed(1)
        return String(Math.round(mb))
    }

    function formatUnit(mb) {
        return mb >= gbThresholdMB ? "GB" : "MB"
    }

    function formatRangeLabel(mb) {
        if (mb >= gbThresholdMB)
            return (mb / gbThresholdMB).toFixed(1) + " GB"
        return Math.round(mb) + " MB"
    }

    function syncTextFromValue() {
        if (sizeInput.activeFocus) {
            sizeInput.text = String(Math.round(sizeSlider.value))
            unitLabel.text = "MB"
        } else {
            sizeInput.text = formatAmount(sizeSlider.value)
            unitLabel.text = formatUnit(sizeSlider.value)
        }
    }

    function applyTextToSlider() {
        if (root.updatingFromSlider)
            return
        var n = parseInt(sizeInput.text, 10)
        if (isNaN(n)) {
            root.invalidInput = true
            return
        }
        if (n < from)
            n = from
        if (n > to)
            n = to
        root.invalidInput = false
        root.updatingFromText = true
        sizeSlider.value = n
        root.updatingFromText = false
        root.syncTextFromValue()
    }

    WizardFieldLabel { label: root.label; visible: root.label !== "" }

    RowLayout {
        width: parent.width
        spacing: 16
        Slider {
            id: sizeSlider
            Layout.fillWidth: true
            stepSize: 1
            onMoved: {
                if (root.updatingFromText)
                    return
                root.updatingFromSlider = true
                root.syncTextFromValue()
                root.updatingFromSlider = false
            }
            onValueChanged: {
                if (root.updatingFromText)
                    return
                root.updatingFromSlider = true
                root.syncTextFromValue()
                root.updatingFromSlider = false
            }
        }
        Rectangle {
            width: 100
            height: 38
            radius: 8
            color: Theme.bgInput
            border.color: root.invalidInput ? Theme.danger
                        : (sizeInput.activeFocus ? Theme.borderFocus : Theme.border)
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 4

                TextInput {
                    id: sizeInput
                    Layout.fillWidth: true
                    verticalAlignment: TextInput.AlignVCenter
                    color: Theme.accent
                    font { pixelSize: 13; bold: true; family: Theme.fontFamily }
                    validator: IntValidator {
                        bottom: sizeSlider.from
                        top: sizeSlider.to
                    }
                    selectByMouse: true
                    onActiveFocusChanged: root.syncTextFromValue()
                    Keys.onReturnPressed: root.applyTextToSlider()
                    onEditingFinished: root.applyTextToSlider()
                }

                Text {
                    id: unitLabel
                    text: "MB"
                    color: Theme.textMuted
                    font { pixelSize: 13; bold: true; family: Theme.fontFamily }
                    Layout.alignment: Qt.AlignVCenter
                }
            }
        }
    }

    Text {
        visible: root.invalidInput
        text: "Enter a value between " + formatRangeLabel(from) + " and " + formatRangeLabel(to)
        color: Theme.danger
        font { pixelSize: 12; family: Theme.fontFamily }
        width: parent.width
        wrapMode: Text.Wrap
    }

    Text {
        visible: root.hintText !== "" && !root.invalidInput
        text: root.hintText
        color: Theme.textMuted
        font { pixelSize: 12; family: Theme.fontFamily }
        width: parent.width
        wrapMode: Text.Wrap
    }

    Component.onCompleted: root.syncTextFromValue()
    onToChanged: {
        if (sizeSlider.value > to)
            sizeSlider.value = to
        root.syncTextFromValue()
    }
}
