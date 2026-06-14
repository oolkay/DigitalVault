import QtQuick
import DigitalVault

Item {
    id: root

    property Item sourceItem: null
    property bool active: false
    property bool dismissible: false
    property bool blocking: false

    signal backdropClicked()

    anchors.fill: parent
    visible: active
    z: 200

    Rectangle {
        anchors.fill: parent
        color: root.blocking ? Qt.rgba(0, 0, 0, 0.65) : Qt.rgba(0, 0, 0, 0.52)
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.active
        onClicked: {
            if (root.blocking)
                return
            if (root.dismissible)
                root.backdropClicked()
        }
    }
}
