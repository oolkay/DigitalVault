import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DigitalVault

Item {
    id: root
    implicitWidth: 500
    implicitHeight: mainCol.implicitHeight

    property string wizardIcon: "W"
    property string wizardTitle: "Wizard"
    property color gradientEndColor: Theme.warning
    property int stepCount: 3
    property string cancelLabel: "Cancel"
    property string nextLabel: "Next →"
    property string finalLabel: "Finish"
    property bool finalButtonDanger: false
    property int finalButtonWidth: 180
    property bool primaryEnabled: true
    property int progressStep: wizardStack.currentIndex

    readonly property alias currentIndex: wizardStack.currentIndex
    readonly property alias stackLayout: wizardStack
    property alias resultText: resultLabel

    signal backRequested()
    signal primaryRequested()

    function resetToFirstStep() {
        wizardStack.currentIndex = 0
        resultLabel.visible = false
    }

    default property alias steps: wizardStack.children

    Column {
        id: mainCol
        width: root.width
        spacing: 0

        Rectangle {
            width: parent.width
            height: 4
            radius: Theme.radius
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Theme.accent }
                GradientStop { position: 1.0; color: root.gradientEndColor }
            }
        }

        Column {
            id: contentCol
            width: parent.width
            spacing: 20
            topPadding: 20
            bottomPadding: 24
            leftPadding: 28
            rightPadding: 28

            readonly property real innerWidth: width - leftPadding - rightPadding

            Row {
                width: contentCol.innerWidth
                spacing: 10
                Text {
                    text: root.wizardIcon
                    font.pixelSize: 20
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: root.wizardTitle
                    color: Theme.textPrimary
                    font { pixelSize: 18; bold: true; family: "Inter" }
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Row {
                width: contentCol.innerWidth
                spacing: 8
                Repeater {
                    model: root.stepCount
                    Rectangle {
                        width: (parent.width - (root.stepCount - 1) * 8) / root.stepCount
                        height: 4
                        radius: 2
                        color: root.progressStep >= index ? Theme.accent : Theme.border
                        Behavior on color { ColorAnimation { duration: 300 } }
                    }
                }
            }

            StackLayout {
                id: wizardStack
                width: contentCol.innerWidth
                clip: true
                implicitHeight: currentItem ? currentItem.implicitHeight : 0
                currentIndex: 0
            }

            Item {
                width: contentCol.innerWidth
                height: resultLabel.visible ? resultLabel.implicitHeight : 0
                clip: true

                Text {
                    id: resultLabel
                    width: parent.width
                    visible: false
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                    font { pixelSize: 12; bold: true; family: "Inter" }
                    color: Theme.danger
                }
            }

            Item {
                width: contentCol.innerWidth
                height: 40
                Row {
                    anchors.right: parent.right
                    spacing: 16

                    Rectangle {
                        width: 100
                        height: 38
                        radius: 19
                        color: "transparent"
                        border.color: Theme.border
                        border.width: 1
                        Text {
                            anchors.centerIn: parent
                            text: root.progressStep === 0 ? root.cancelLabel : "← Back"
                            color: Theme.textSecondary
                            font { pixelSize: 12; bold: true; family: "Inter" }
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.backRequested()
                        }
                    }

                    Rectangle {
                        id: primaryBtn
                        width: root.progressStep === root.stepCount - 1
                               ? root.finalButtonWidth : 120
                        height: 38
                        radius: 19
                        color: root.progressStep === root.stepCount - 1 && root.finalButtonDanger
                               ? Theme.danger : Theme.accent
                        opacity: root.primaryEnabled ? 1.0 : 0.45
                        Behavior on width { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                        Behavior on color { ColorAnimation { duration: 250 } }
                        Behavior on opacity { NumberAnimation { duration: 150 } }
                        Text {
                            anchors.centerIn: parent
                            text: root.progressStep === root.stepCount - 1
                                  ? root.finalLabel : root.nextLabel
                            color: "white"
                            font { pixelSize: 12; bold: true; family: "Inter" }
                        }
                        MouseArea {
                            anchors.fill: parent
                            enabled: root.primaryEnabled
                            cursorShape: root.primaryEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: root.primaryRequested()
                        }
                    }
                }
            }
        }
    }
}
