import QtQuick
import DigitalVault

Text {
    required property string label
    text: label
    color: Theme.textSecondary
    font { pixelSize: 11; bold: true; family: "Inter" }
}
