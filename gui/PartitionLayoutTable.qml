import QtQuick
import QtQuick.Controls
import DigitalVault

Column {
    id: root
    spacing: 8
    width: parent ? parent.width : 400

    property var partitions: []
    property string selectedPartitionPath: ""

    readonly property int rowHeight: 34
    readonly property int hPad: 8
    readonly property int colGap: 8

    signal rowClicked(string path)

    readonly property real colPartition: 0.20
    readonly property real colName: 0.13
    readonly property real colFs: 0.10
    readonly property real colMount: 0.24
    readonly property real colSize: 0.09
    readonly property real colUsed: 0.09
    readonly property real colUnused: 0.10

    function colWidth(fraction) {
        return Math.floor(root.width * fraction)
    }

    readonly property real tableContentWidth:
        colWidth(colPartition) + colWidth(colName) + colWidth(colFs) + colWidth(colMount)
        + colWidth(colSize) + colWidth(colUsed) + colWidth(colUnused) + colGap * 6

    function mountCellText(part) {
        const mp = part.mountpoint || ""
        return mp === "" ? "(not mounted)" : mp
    }

    ScrollView {
        id: tableScroll
        width: parent.width
        height: Math.min(220, headerBlock.height + bodyCol.implicitHeight + 4)
        clip: true
        ScrollBar.horizontal.policy: tableContentWidth > width ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        Column {
            id: tableCol
            width: Math.max(tableScroll.availableWidth, root.tableContentWidth)
            spacing: 4

            Rectangle {
                id: headerBlock
                width: parent.width
                height: root.rowHeight
                radius: 6
                color: Theme.bgInput
                border.color: Theme.border
                border.width: 1

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: root.hPad
                    anchors.rightMargin: root.hPad
                    spacing: root.colGap

                    Text {
                        width: root.colWidth(root.colPartition)
                        height: parent.height
                        verticalAlignment: Text.AlignVCenter
                        text: "partition"
                        color: Theme.textMuted
                        font { pixelSize: 9; bold: true; family: "Inter" }
                        elide: Text.ElideRight
                    }
                    Text {
                        width: root.colWidth(root.colName)
                        height: parent.height
                        verticalAlignment: Text.AlignVCenter
                        text: "name"
                        color: Theme.textMuted
                        font { pixelSize: 9; bold: true; family: "Inter" }
                        elide: Text.ElideRight
                    }
                    Text {
                        width: root.colWidth(root.colFs)
                        height: parent.height
                        verticalAlignment: Text.AlignVCenter
                        text: "filesystem"
                        color: Theme.textMuted
                        font { pixelSize: 9; bold: true; family: "Inter" }
                        elide: Text.ElideRight
                    }
                    Text {
                        width: root.colWidth(root.colMount)
                        height: parent.height
                        verticalAlignment: Text.AlignVCenter
                        text: "mount point"
                        color: Theme.textMuted
                        font { pixelSize: 9; bold: true; family: "Inter" }
                        elide: Text.ElideRight
                    }
                    Text {
                        width: root.colWidth(root.colSize)
                        height: parent.height
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignRight
                        text: "size"
                        color: Theme.textMuted
                        font { pixelSize: 9; bold: true; family: "Inter" }
                        elide: Text.ElideRight
                    }
                    Text {
                        width: root.colWidth(root.colUsed)
                        height: parent.height
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignRight
                        text: "used"
                        color: Theme.textMuted
                        font { pixelSize: 9; bold: true; family: "Inter" }
                        elide: Text.ElideRight
                    }
                    Text {
                        width: root.colWidth(root.colUnused)
                        height: parent.height
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignRight
                        text: "unused"
                        color: Theme.textMuted
                        font { pixelSize: 9; bold: true; family: "Inter" }
                        elide: Text.ElideRight
                    }
                }
            }

            Column {
                id: bodyCol
                width: parent.width
                spacing: 4

                Repeater {
                    model: root.partitions

                    delegate: Rectangle {
                        required property var modelData
                        required property int index

                        width: bodyCol.width
                        height: root.rowHeight
                        radius: 6
                        color: modelData.path === root.selectedPartitionPath
                               ? Theme.accentGlow
                               : (index % 2 === 0 ? Theme.bgOverlay : "transparent")
                        border.color: modelData.path === root.selectedPartitionPath
                                      ? Theme.accent : "transparent"
                        border.width: modelData.path === root.selectedPartitionPath ? 1 : 0

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: root.hPad
                            anchors.rightMargin: root.hPad
                            spacing: root.colGap

                            Text {
                                width: root.colWidth(root.colPartition)
                                height: parent.height
                                verticalAlignment: Text.AlignVCenter
                                text: modelData.path || ""
                                color: Theme.textPrimary
                                font { pixelSize: 10; family: "Inter" }
                                elide: Text.ElideMiddle
                            }
                            Text {
                                width: root.colWidth(root.colName)
                                height: parent.height
                                verticalAlignment: Text.AlignVCenter
                                text: modelData.roleLabel || modelData.label || ""
                                color: Theme.textSecondary
                                font { pixelSize: 10; family: "Inter" }
                                elide: Text.ElideRight
                            }
                            Text {
                                width: root.colWidth(root.colFs)
                                height: parent.height
                                verticalAlignment: Text.AlignVCenter
                                text: modelData.fsLabel || "—"
                                color: Theme.textSecondary
                                font { pixelSize: 10; family: "Inter" }
                                elide: Text.ElideRight
                            }
                            Text {
                                width: root.colWidth(root.colMount)
                                height: parent.height
                                verticalAlignment: Text.AlignVCenter
                                text: root.mountCellText(modelData)
                                color: Theme.textMuted
                                font { pixelSize: 10; family: "Inter" }
                                elide: Text.ElideMiddle
                            }
                            Text {
                                width: root.colWidth(root.colSize)
                                height: parent.height
                                verticalAlignment: Text.AlignVCenter
                                horizontalAlignment: Text.AlignRight
                                text: modelData.size || "—"
                                color: Theme.textPrimary
                                font { pixelSize: 10; family: "Inter" }
                                elide: Text.ElideRight
                            }
                            Text {
                                width: root.colWidth(root.colUsed)
                                height: parent.height
                                verticalAlignment: Text.AlignVCenter
                                horizontalAlignment: Text.AlignRight
                                text: modelData.usedLabel || "—"
                                color: Theme.textSecondary
                                font { pixelSize: 10; family: "Inter" }
                                elide: Text.ElideRight
                            }
                            Text {
                                width: root.colWidth(root.colUnused)
                                height: parent.height
                                verticalAlignment: Text.AlignVCenter
                                horizontalAlignment: Text.AlignRight
                                text: modelData.unusedLabel || "—"
                                color: Theme.textSecondary
                                font { pixelSize: 10; family: "Inter" }
                                elide: Text.ElideRight
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: root.rowClicked(modelData.path || "")
                        }
                    }
                }
            }
        }
    }
}
