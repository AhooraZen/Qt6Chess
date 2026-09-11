import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property var gameController: null
    property bool flipped: root.gameController ? root.gameController.flipped : false
    property int boardTheme: 0 // 0: Emerald, 1: Wood, 2: Slate, 3: Midnight

    // Color theme palettes
    readonly property var themePalettes: [
        { light: "#eeeed2", dark: "#769656" }, // Emerald
        { light: "#f0d9b5", dark: "#b58863" }, // Wood
        { light: "#dee3e6", dark: "#8ca2ad" }, // Slate
        { light: "#c4cfa1", dark: "#4d6a79" }  // Midnight
    ]

    property color lightSquareColor: themePalettes[boardTheme] ? themePalettes[boardTheme].light : "#eeeed2"
    property color darkSquareColor: themePalettes[boardTheme] ? themePalettes[boardTheme].dark : "#769656"
    property color selectedColor: "#baca2b"
    property color lastMoveColor: "#f5f682"
    property color checkColor: "#e05353"
    property color dotColor: Qt.rgba(0, 0, 0, 0.22)

    implicitWidth: 480
    implicitHeight: 480

    Rectangle {
        id: boardBg
        anchors.fill: parent
        color: "#262522"
        radius: 4

        // 8x8 Board Area
        Item {
            id: boardArea
            anchors.fill: parent
            anchors.margins: 4

            Repeater {
                id: boardRepeater
                model: root.gameController ? root.gameController.boardModel : null

                delegate: Rectangle {
                    id: squareItem
                    required property int index
                    required property int squareIndex
                    required property string squareName
                    required property string pieceCode
                    required property bool isLightSquare
                    required property bool isSelected
                    required property bool isLegalTarget
                    required property bool isLastMove
                    required property bool isInCheck

                    // Board ranks & files
                    readonly property int rank: Math.floor(squareIndex / 8)
                    readonly property int file: squareIndex % 8

                    // Coordinate mapping based on flip
                    readonly property int displayCol: root.flipped ? (7 - file) : file
                    readonly property int displayRow: root.flipped ? rank : (7 - rank)

                    width: boardArea.width / 8
                    height: boardArea.height / 8
                    x: displayCol * width
                    y: displayRow * height

                    color: {
                        if (isInCheck) return root.checkColor;
                        if (isSelected) return root.selectedColor;
                        if (isLastMove) return root.lastMoveColor;
                        return isLightSquare ? root.lightSquareColor : root.darkSquareColor;
                    }

                    // Rank & File coordinate labels
                    Text {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.margins: 2
                        text: (displayCol === 0) ? (rank + 1).toString() : ""
                        font.pixelSize: Math.max(9, parent.width * 0.18)
                        font.bold: true
                        color: squareItem.isLightSquare ? root.darkSquareColor : root.lightSquareColor
                        visible: text.length > 0
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 2
                        text: (displayRow === 7) ? String.fromCharCode(97 + file) : ""
                        font.pixelSize: Math.max(9, parent.width * 0.18)
                        font.bold: true
                        color: squareItem.isLightSquare ? root.darkSquareColor : root.lightSquareColor
                        visible: text.length > 0
                    }

                    // Legal destination indicator: dot for empty, ring for capture
                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width * 0.32
                        height: width
                        radius: width / 2
                        color: root.dotColor
                        visible: isLegalTarget && pieceCode.length === 0
                    }

                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width * 0.88
                        height: width
                        radius: width / 2
                        color: "transparent"
                        border.color: root.dotColor
                        border.width: Math.max(3, parent.width * 0.08)
                        visible: isLegalTarget && pieceCode.length > 0
                    }

                    // Piece Graphic
                    Image {
                        id: pieceImg
                        anchors.centerIn: parent
                        width: parent.width * 0.92
                        height: parent.height * 0.92
                        sourceSize.width: width * 2
                        sourceSize.height: height * 2
                        fillMode: Image.PreserveAspectFit
                        visible: pieceCode.length > 0
                        source: pieceCode.length > 0 ? "qrc:/qt/qml/Qt6Chess/UI/resources/pieces/cburnett/" + pieceCode + ".svg" : ""
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if (root.gameController) {
                                root.gameController.selectSquare(squareIndex);
                            }
                        }
                    }
                }
            }

            // Arrow Overlay for tactical lines & best moves
            ArrowOverlay {
                id: arrows
                anchors.fill: parent
                gameController: root.gameController
                flipped: root.flipped
            }
        }
    }
}
