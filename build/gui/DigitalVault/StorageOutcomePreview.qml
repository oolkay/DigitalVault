import QtQuick
import DigitalVault

Column {
    id: root
    spacing: 12
    width: parent ? parent.width : 360

    property var preview: ({})

    function asList(value) {
        if (value === undefined || value === null)
            return []
        if (Array.isArray(value))
            return value
        if (typeof value.length === "number" && typeof value !== "string") {
            var out = []
            for (var i = 0; i < value.length; ++i)
                out.push(value[i])
            return out
        }
        return []
    }

    readonly property var deletedItems: asList(preview.deleted)
    readonly property var keptItems: asList(preview.kept)
    readonly property var registryItems: asList(preview.registry)

    Column {
        id: deletedSection
        width: parent.width
        spacing: 6
        visible: root.deletedItems.length > 0
        Text {
            text: "Deleted"
            color: Theme.danger
            font { pixelSize: 12; bold: true; family: Theme.fontFamily }
        }
        Repeater {
            model: root.deletedItems
            delegate: Text {
                required property var modelData
                width: deletedSection.width
                wrapMode: Text.Wrap
                text: "• " + modelData
                color: Theme.textPrimary
                font { pixelSize: 11; family: Theme.fontFamily }
            }
        }
    }

    Column {
        id: keptSection
        width: parent.width
        spacing: 6
        visible: root.keptItems.length > 0
        Text {
            text: "Kept"
            color: Theme.success
            font { pixelSize: 12; bold: true; family: Theme.fontFamily }
        }
        Repeater {
            model: root.keptItems
            delegate: Text {
                required property var modelData
                width: keptSection.width
                wrapMode: Text.Wrap
                text: "• " + modelData
                color: Theme.textPrimary
                font { pixelSize: 11; family: Theme.fontFamily }
            }
        }
    }

    Column {
        id: registrySection
        width: parent.width
        spacing: 6
        visible: root.registryItems.length > 0
        Text {
            text: "Registry"
            color: Theme.textSecondary
            font { pixelSize: 12; bold: true; family: Theme.fontFamily }
        }
        Repeater {
            model: root.registryItems
            delegate: Text {
                required property var modelData
                width: registrySection.width
                wrapMode: Text.Wrap
                text: "• " + modelData
                color: Theme.textPrimary
                font { pixelSize: 11; family: Theme.fontFamily }
            }
        }
    }

    Text {
        width: parent.width
        wrapMode: Text.Wrap
        visible: preview.after !== undefined && preview.after !== ""
        text: "After: " + (preview.after || "")
        color: Theme.textSecondary
        font { pixelSize: 11; family: Theme.fontFamily }
    }
}
