import QtQuick
import DigitalVault

Text {
    required property string title
    text: title
    color: Theme.textPrimary
    font { pixelSize: 14; bold: true; family: "Inter" }
}
