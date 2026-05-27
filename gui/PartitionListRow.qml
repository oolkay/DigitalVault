import QtQuick
import DigitalVault

Rectangle {
    id: root
    width: parent ? parent.width : 400
    height: rowContent.implicitHeight + 20
    radius: 8
    clip: true
    color: isSelected ? Theme.accentGlow : Theme.bgOverlay
    border.color: isSelected ? Theme.accent : Theme.border
    border.width: isSelected ? 2 : 1

    property var partitionData: ({})
    property bool isSelected: false
    readonly property int sizeColumnWidth: 64

    signal rowClicked()

    MouseArea {
        anchors.fill: parent
        onClicked: root.rowClicked()
    }

    Column {
        id: rowContent
        anchors.fill: parent
        anchors.margins: 10
        spacing: 4

        Row {
            width: parent.width
            spacing: 10

            Rectangle {
                width: 8
                height: 8
                radius: 4
                anchors.verticalCenter: parent.verticalCenter
                color: {
                    const role = partitionData.role || ""
                    if (role === "luks") return Theme.danger
                    if (role === "container") return Theme.textMuted
                    return Theme.success
                }
            }

            Rectangle {
                id: roleBadge
                anchors.verticalCenter: parent.verticalCenter
                height: 20
                width: Math.min(80, fsLbl.implicitWidth + 12)
                radius: 10
                color: Theme.bgInput
                border.color: Theme.border
                Text {
                    id: fsLbl
                    anchors.centerIn: parent
                    width: parent.width - 8
                    horizontalAlignment: Text.AlignHCenter
                    text: partitionData.fsLabel || "—"
                    color: Theme.textSecondary
                    font { pixelSize: 9; family: "Inter" }
                    elide: Text.ElideRight
                }
            }

            Column {
                width: Math.max(0, parent.width - 8 - 10 - roleBadge.width - sizeColumnWidth)
                spacing: 2
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    width: parent.width
                    text: partitionData.roleLabel || partitionData.label || "Partition"
                    color: Theme.textPrimary
                    font { pixelSize: 12; bold: true; family: "Inter" }
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    text: partitionData.path || ""
                    color: Theme.textSecondary
                    font { pixelSize: 10; family: "Inter" }
                    elide: Text.ElideMiddle
                }
            }

            Column {
                id: sizeCol
                width: sizeColumnWidth
                spacing: 2
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignRight
                    text: partitionData.size || "—"
                    color: Theme.textPrimary
                    font { pixelSize: 11; bold: true; family: "Inter" }
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignRight
                    text: partitionData.percentOfDisk || ""
                    color: Theme.textMuted
                    font { pixelSize: 9; family: "Inter" }
                    elide: Text.ElideRight
                    visible: text !== ""
                }
            }
        }

        Text {
            width: parent.width
            text: partitionData.mountDisplay || ""
            color: Theme.textMuted
            font { pixelSize: 9; family: "Inter" }
            elide: Text.ElideRight
        }
    }
}
