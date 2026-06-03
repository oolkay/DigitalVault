import QtQuick
import DigitalVault

Text {
    required property string title
    text: title
    color: Theme.textPrimary
    font { pixelSize: 17; bold: true; family: Theme.fontFamily }
}
