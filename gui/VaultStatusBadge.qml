import QtQuick
import DigitalVault

Rectangle {
    id: root
    height: 26
    radius: 13
    color: {
        if (vaultController.isToggling && isActiveVault)
            return Theme.warningBg
        if (isUnlocked)
            return Theme.successBg
        return Theme.dangerBg
    }
    border.color: {
        if (vaultController.isToggling && isActiveVault)
            return Theme.warning
        if (isUnlocked)
            return Theme.success
        return Theme.danger
    }
    border.width: 1

    property bool isUnlocked: false
    property bool isActiveVault: true

    implicitWidth: statusText.implicitWidth + 20

    Text {
        id: statusText
        anchors.centerIn: parent
        text: {
            if (vaultController.isToggling && root.isActiveVault)
                return root.isUnlocked ? "Locking…" : "Unlocking…"
            return root.isUnlocked ? "Unlocked" : "Locked"
        }
        color: {
            if (vaultController.isToggling && root.isActiveVault)
                return Theme.warning
            return root.isUnlocked ? Theme.success : Theme.danger
        }
        font { pixelSize: 11; bold: true; family: Theme.fontFamily }
    }
}
