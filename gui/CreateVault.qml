import QtQuick
import DigitalVault

// Legacy page — setup and loop creation moved to Vault Manager.
Item {
    Component.onCompleted: vaultController.navigateToPage("VaultManager.qml")

    Text {
        anchors.centerIn: parent
        text: "Opening Vault Manager…"
        color: Theme.textSecondary
        font { pixelSize: 14; family: "Inter" }
    }
}
