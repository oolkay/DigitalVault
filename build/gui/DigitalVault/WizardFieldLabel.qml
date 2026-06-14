import QtQuick
import DigitalVault

Text {
    required property string label
    text: label
    color: Theme.textSecondary
    font { pixelSize: 13; bold: true; family: Theme.fontFamily }
}
