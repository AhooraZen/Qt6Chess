import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property var gameController: null
    property bool flipped: gameController ? gameController.flipped : false

    // Color theme
    property color lightSquareColor: "#eeeed2"
    property color darkSquareColor: "#769656"
    property color selectedColor: "#baca2b"
    property color lastMoveColor: "#f5f682"
    property color checkColor: "#e05353"
    property color dotColor: Qt.rgba(0, 0, 0, 0.22)

    width: Math.min(parent.width, parent.height)
    height: width

    Rectangle {
        id: boardBg
        anchors.fill: parent
        color: "#262522"
        radius: 4

        // 8x8 Board Grid
        Grid {
            id: grid
            anchors.fill: parent
            anchors.margins: 4
            rows: 8
            columns: 8

            Repeater {
                model: 64

                delegate: Rectangle {
                    id: squareItem
                    required property int index

                    width: (grid.width) / 8
                    height: (grid.height) / 8

                    // Coordinate mapping based on flip
                    readonly property int row: Math.floor(index / 8)
                    readonly property int col: index % 8
                    readonly property int rank: root.flipped ? row : (7 - row)
                    readonly property int file: root.flipped ? (7 - col) : col
                    readonly property int sqIndex: rank * 8 + file

                    // Data from model
                    readonly property var sqData: root.gameController ? root.gameController.boardModel.getSquareData(sqIndex) : null
                    readonly property bool isLight: ((file + rank) % 2 !== 0)
                    readonly property bool isSelected: sqData ? sqData.isSelected : false
                    readonly property bool isLastMove: sqData ? sqData.isLastMove : false
                    readonly property bool isInCheck: sqData ? sqData.isInCheck : false
                    readonly property bool isLegalTarget: sqData ? sqData.isLegalTarget : false
                    readonly property string pieceCode: sqData ? sqData.pieceCode : ""

                    color: {
                        if (isInCheck) return root.checkColor;
                        if (isSelected) return root.selectedColor;
                        if (isLastMove) return root.lastMoveColor;
                        return isLight ? root.lightSquareColor : root.darkSquareColor;
                    }

                    // Rank & File coordinate labels
                    Text {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.margins: 2
                        text: (file === (root.flipped ? 7 : 0)) ? (rank + 1).toString() : ""
                        font.pixelSize: Math.max(9, parent.width * 0.18)
                        font.bold: true
                        color: squareItem.isLight ? root.darkSquareColor : root.lightSquareColor
                        visible: text.length > 0
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 2
                        text: (rank === (root.flipped ? 7 : 0)) ? String.fromCharCode(97 + file) : ""
                        font.pixelSize: Math.max(9, parent.width * 0.18)
                        font.bold: true
                        color: squareItem.isLight ? root.darkSquareColor : root.lightSquareColor
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
                                root.gameController.selectSquare(sqIndex);
                            }
                        }
                    }
                }
            }
        }

        // Arrow Overlay for tactical lines & best moves
        ArrowOverlay {
            id: arrows
            anchors.fill: grid
            gameController: root.gameController
            flipped: root.flipped
        }
    }
}
