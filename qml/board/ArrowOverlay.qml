import QtQuick

Canvas {
    id: canvas
    property var gameController: null
    property bool flipped: false

    property string primaryFrom: ""
    property string primaryTo: ""
    property string secondaryFrom: ""
    property string secondaryTo: ""

    Connections {
        target: gameController ? gameController.uciController : null
        function onPrimaryArrowChanged(fromSq, toSq) {
            canvas.primaryFrom = fromSq;
            canvas.primaryTo = toSq;
            canvas.requestPaint();
        }
        function onSecondaryArrowChanged(fromSq, toSq) {
            canvas.secondaryFrom = fromSq;
            canvas.secondaryTo = toSq;
            canvas.requestPaint();
        }
    }

    onFlippedChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    function squareToCoord(sq) {
        if (!sq || sq.length < 2) return null;
        var file = sq.charCodeAt(0) - 97; // 'a' -> 0
        var rank = parseInt(sq.charAt(1)) - 1; // '1' -> 0

        var col = flipped ? (7 - file) : file;
        var row = flipped ? rank : (7 - rank);

        var sqW = width / 8.0;
        var sqH = height / 8.0;
        return {
            x: col * sqW + sqW / 2.0,
            y: row * sqH + sqH / 2.0,
            size: sqW
        };
    }

    function drawArrow(ctx, fromSq, toSq, color, widthRatio) {
        var start = squareToCoord(fromSq);
        var end = squareToCoord(toSq);
        if (!start || !end) return;

        var dx = end.x - start.x;
        var dy = end.y - start.y;
        var dist = Math.sqrt(dx * dx + dy * dy);
        if (dist < 1) return;

        var normX = dx / dist;
        var normY = dy / dist;

        var arrowWidth = start.size * widthRatio;
        var headSize = start.size * (widthRatio * 2.2);

        // Shorten endpoint slightly so arrowhead sits nicely in center
        var headBaseX = end.x - normX * (headSize * 0.8);
        var headBaseY = end.y - normY * (headSize * 0.8);

        ctx.save();
        ctx.fillStyle = color;
        ctx.strokeStyle = color;
        ctx.lineWidth = arrowWidth;
        ctx.lineCap = "round";

        // Shaft
        ctx.beginPath();
        ctx.moveTo(start.x, start.y);
        ctx.lineTo(headBaseX, headBaseY);
        ctx.stroke();

        // Arrow head (triangle)
        var perpX = -normY;
        var perpY = normX;

        ctx.beginPath();
        ctx.moveTo(end.x, end.y);
        ctx.lineTo(headBaseX + perpX * headSize * 0.6, headBaseY + perpY * headSize * 0.6);
        ctx.lineTo(headBaseX - perpX * headSize * 0.6, headBaseY - perpY * headSize * 0.6);
        ctx.closePath();
        ctx.fill();

        ctx.restore();
    }

    onPaint: {
        var ctx = getContext("2d");
        ctx.clearRect(0, 0, width, height);

        if (secondaryFrom && secondaryTo) {
            drawArrow(ctx, secondaryFrom, secondaryTo, Qt.rgba(0.02, 0.71, 0.83, 0.65), 0.16);
        }
        if (primaryFrom && primaryTo) {
            drawArrow(ctx, primaryFrom, primaryTo, Qt.rgba(0.13, 0.77, 0.37, 0.80), 0.22);
        }
    }
}
