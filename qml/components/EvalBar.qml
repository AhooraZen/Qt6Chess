import QtQuick

Item {
    id: root
    property var uciController: null
    property bool flipped: false

    width: 28
    height: parent ? parent.height : 400

    readonly property double rawEval: uciController ? uciController.currentEval : 0.0
    readonly property bool isMate: uciController ? uciController.isMate : false
    readonly property int mateIn: uciController ? uciController.mateIn : 0

    // White winning percentage [0.05, 0.95]
    readonly property double whiteRatio: {
        if (isMate) {
            return (mateIn > 0) ? 0.98 : 0.02;
        }
        var clamped = Math.max(-10.0, Math.min(10.0, rawEval));
        return 1.0 / (1.0 + Math.pow(10, -clamped / 4.0));
    }

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: 4
        color: "#21201d"
        clip: true

        // White portion
        Rectangle {
            id: whiteFill
            width: parent.width
            color: "#ffffff"

            anchors.bottom: root.flipped ? undefined : parent.bottom
            anchors.top: root.flipped ? parent.top : undefined

            height: parent.height * root.whiteRatio

            Behavior on height {
                NumberAnimation { duration: 280; easing.type: Easing.OutCubic }
            }
        }

        // Eval text badge
        Text {
            id: evalLabel
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: (root.whiteRatio > 0.5 !== root.flipped) ? parent.bottom : undefined
            anchors.top: (root.whiteRatio > 0.5 !== root.flipped) ? undefined : parent.top
            anchors.margins: 4

            font.pixelSize: 10
            font.bold: true
            color: (root.whiteRatio > 0.5 !== root.flipped) ? "#333333" : "#ffffff"

            text: {
                if (root.isMate) {
                    return "M" + Math.abs(root.mateIn);
                }
                var prefix = (root.rawEval > 0) ? "+" : "";
                return prefix + root.rawEval.toFixed(1);
            }
        }
    }
}
