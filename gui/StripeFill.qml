import QtQuick
import DigitalVault

Item {
    id: root

    property color baseColor: Theme.success
    property real stripeOpacity: 0.35
    property int stripeSpacing: 5

    Rectangle {
        anchors.fill: parent
        color: baseColor
        opacity: 0.28
    }

    Canvas {
        id: stripeCanvas
        anchors.fill: parent

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.clearRect(0, 0, width, height)
            ctx.strokeStyle = Qt.rgba(1, 1, 1, stripeOpacity)
            ctx.lineWidth = 1
            const step = Math.max(3, stripeSpacing)
            for (let i = -height; i < width + height; i += step) {
                ctx.beginPath()
                ctx.moveTo(i, height)
                ctx.lineTo(i + height, 0)
                ctx.stroke()
            }
        }

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Component.onCompleted: requestPaint()

        Connections {
            target: root
            function onBaseColorChanged() { stripeCanvas.requestPaint() }
            function onStripeOpacityChanged() { stripeCanvas.requestPaint() }
            function onStripeSpacingChanged() { stripeCanvas.requestPaint() }
        }
    }
}
