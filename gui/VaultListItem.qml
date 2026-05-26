import QtQuick
import DigitalVault

Rectangle {
    id: root
    width: ListView.view ? ListView.view.width : 280
    height: 72
    radius: 10
    color: isSelected ? Theme.navActive : Theme.bgOverlay
    border.color: isSelected ? Theme.accent : Theme.border
    border.width: isSelected ? 2 : 1

    property var vaultData: ({})
    property bool isSelected: vaultController.selectedDevice === (vaultData.path || "")

    signal clicked()

    Behavior on color { ColorAnimation { duration: 150 } }
    Behavior on border.color { ColorAnimation { duration: 150 } }

    Rectangle {
        width: 3
        radius: 2
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom; leftMargin: 0 }
        color: Theme.accent
        visible: isSelected
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    Row {
        anchors { fill: parent; leftMargin: 14; rightMargin: 14; topMargin: 12; bottomMargin: 12 }
        spacing: 12

        Rectangle {
            width: 44
            height: 44
            radius: 8
            color: isSelected ? Theme.accentGlow : Theme.successBg
            anchors.verticalCenter: parent.verticalCenter
            Text {
                anchors.centerIn: parent
                text: "🔐"
                font.pixelSize: 20
            }
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 44 - 12 - statusDot.width - 8
            spacing: 4

            Text {
                width: parent.width
                elide: Text.ElideRight
                text: vaultData.displayName || vaultData.name || "Vault"
                color: isSelected ? Theme.navActiveText : Theme.textPrimary
                font { pixelSize: 14; bold: true; family: "Inter" }
            }
            Text {
                width: parent.width
                elide: Text.ElideRight
                text: vaultData.sizeLabel || "Encrypted vault"
                color: Theme.textMuted
                font { pixelSize: 11; family: "Inter" }
            }
        }

        Rectangle {
            id: statusDot
            width: 10
            height: 10
            radius: 5
            anchors.verticalCenter: parent.verticalCenter
            color: {
                if (vaultController.isVaultUnlockedForDevice(vaultData.path || ""))
                    return Theme.success
                if (isSelected)
                    return Theme.danger
                return Theme.textMuted
            }
        }
    }
}
