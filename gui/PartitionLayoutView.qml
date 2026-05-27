import QtQuick
import DigitalVault

Column {
    id: root
    spacing: 12
    width: parent ? parent.width : 400

    property var layoutData: ({})
    property string selectedPartitionPath: ""

    signal partitionSelected(string path)

    readonly property var partitions: layoutData.partitions || []

    function segmentPathLabel(part, segmentWidth) {
        const path = part.path || ""
        if (segmentWidth >= 72 || path.length <= 14)
            return path
        const slash = path.lastIndexOf("/")
        return slash >= 0 ? path.substring(slash + 1) : path
    }

    function usageRatio(part) {
        const r = part.usedRatio
        if (r === undefined || r === null || r < 0)
            return -1
        return Math.min(1, Math.max(0, r))
    }

    function roleBaseColor(role) {
        if (role === "luks") return Theme.danger
        if (role === "container") return Theme.textMuted
        return Theme.success
    }

    function roleBgColor(role, selected) {
        if (selected) return Theme.accentGlow
        if (role === "luks") return Theme.dangerBg
        if (role === "container") return Theme.bgOverlay
        return Theme.successBg
    }

    Column {
        width: parent.width
        spacing: 4
        visible: layoutData.diskPath !== undefined && layoutData.diskPath !== ""

        Text {
            width: parent.width
            text: layoutData.diskModel || "Storage device"
            color: Theme.textPrimary
            font { pixelSize: 13; bold: true; family: "Inter" }
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            wrapMode: Text.Wrap
            text: {
                var bits = []
                if (layoutData.diskSize)
                    bits.push(layoutData.diskSize)
                if (layoutData.partitionCount > 0)
                    bits.push(layoutData.partitionCount + " partition"
                              + (layoutData.partitionCount === 1 ? "" : "s"))
                if (layoutData.usedSummary)
                    bits.push(layoutData.usedSummary)
                return bits.join(" · ")
            }
            color: Theme.textSecondary
            font { pixelSize: 11; family: "Inter" }
        }

        Text {
            width: parent.width
            text: layoutData.diskPath || ""
            color: Theme.textMuted
            font { pixelSize: 10; family: "Inter" }
            elide: Text.ElideMiddle
        }
    }

    Rectangle {
        width: parent.width
        height: 60
        radius: 8
        color: Theme.bgInput
        border.color: Theme.border
        border.width: 1
        clip: true
        visible: partitions.length > 0

        Row {
            anchors.fill: parent
            anchors.margins: 2
            spacing: 2

            Repeater {
                model: root.partitions
                delegate: Rectangle {
                    required property var modelData
                    required property int index

                    property real _ratio: {
                        var parts = root.partitions
                        var total = 0
                        for (var i = 0; i < parts.length; ++i)
                            total += parts[i].bytes || 1
                        if (total <= 0) total = parts.length || 1
                        return (modelData.bytes || 1) / total
                    }

                    property real _usage: root.usageRatio(modelData)
                    property bool _hasUsage: _usage >= 0
                    property string _role: modelData.role || "storage"
                    property bool _selected: modelData.path === root.selectedPartitionPath
                    property real segmentWidth: width

                    height: parent.height
                    width: Math.max(48, parent.width * _ratio - 2)
                    color: root.roleBgColor(_role, _selected)
                    border.color: _selected ? Theme.accent : Theme.border
                    border.width: _selected ? 2 : 1
                    clip: true

                    Row {
                        id: usageBar
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 2
                        height: 11
                        spacing: 0
                        visible: _hasUsage

                        StripeFill {
                            height: parent.height
                            width: _hasUsage && _usage > 0
                                   ? Math.max(1, parent.width * _usage) : 0
                            visible: width > 0
                            baseColor: root.roleBaseColor(_role)
                        }

                        Rectangle {
                            height: parent.height
                            width: Math.max(0, usageBar.width - (_hasUsage && _usage > 0
                                        ? Math.max(1, usageBar.width * _usage) : 0))
                            color: Qt.rgba(1, 1, 1, 0.08)
                        }
                    }

                    Column {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 3
                        spacing: 1

                        Text {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            text: root.segmentPathLabel(modelData, segmentWidth)
                            color: Theme.textPrimary
                            font { pixelSize: segmentWidth < 56 ? 7 : 8; family: "Inter" }
                            elide: Text.ElideMiddle
                        }

                        Text {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            text: modelData.size || ""
                            color: Theme.textSecondary
                            font { pixelSize: 7; family: "Inter" }
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.partitionSelected(modelData.path || "")
                    }
                }
            }
        }
    }

    Column {
        width: parent.width
        spacing: 6
        visible: partitions.length > 0

        Flow {
            width: parent.width
            spacing: 14

            Row {
                spacing: 6
                Rectangle {
                    width: 8; height: 8; radius: 2
                    color: Theme.dangerBg
                    border.color: Theme.danger
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: "Encrypted vault"
                    color: Theme.textMuted
                    font { pixelSize: 10; family: "Inter" }
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            Row {
                spacing: 6
                Rectangle {
                    width: 8; height: 8; radius: 2
                    color: Theme.successBg
                    border.color: Theme.success
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: "Normal storage"
                    color: Theme.textMuted
                    font { pixelSize: 10; family: "Inter" }
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            Row {
                spacing: 6
                visible: layoutData.diskType === "loop"
                Rectangle {
                    width: 8; height: 8; radius: 2
                    color: Theme.bgOverlay
                    border.color: Theme.border
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: "Container"
                    color: Theme.textMuted
                    font { pixelSize: 10; family: "Inter" }
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            Row {
                spacing: 6
                StripeFill {
                    width: 14
                    height: 8
                    baseColor: Theme.success
                }
                Text {
                    text: "Used"
                    color: Theme.textMuted
                    font { pixelSize: 10; family: "Inter" }
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            Row {
                spacing: 6
                Rectangle {
                    width: 14; height: 8; radius: 2
                    color: Qt.rgba(1, 1, 1, 0.08)
                    border.color: Theme.border
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: "Free"
                    color: Theme.textMuted
                    font { pixelSize: 10; family: "Inter" }
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        Text {
            width: parent.width
            wrapMode: Text.Wrap
            text: "Usage shown when a partition is mounted or the vault is unlocked on the Dashboard."
            color: Theme.textMuted
            font { pixelSize: 9; family: "Inter" }
        }
    }

    Text {
        width: parent.width
        visible: partitions.length === 0
        wrapMode: Text.Wrap
        text: "No partition layout reported for this device."
        color: Theme.textMuted
        font { pixelSize: 11; family: "Inter" }
    }
}
