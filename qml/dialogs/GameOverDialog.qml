import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root
    property var gameController: null

    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        color: "#262522"
        radius: 10
        border.color: "#38bdf8"
        border.width: 2
    }

    contentItem: ColumnLayout {
        spacing: 16

        Text {
            text: "Game Over"
            font.bold: true
            font.pixelSize: 20
            color: "#ffffff"
            Layout.alignment: Qt.AlignHCenter
        }

        Text {
            text: root.gameController ? root.gameController.gameResult : ""
            font.pixelSize: 15
            color: "#e2e8f0"
            Layout.alignment: Qt.AlignHCenter
        }

        RowLayout {
            spacing: 12
            Layout.alignment: Qt.AlignHCenter

            Button {
                text: "New Game"
                highlighted: true
                onClicked: {
                    if (root.gameController) root.gameController.newGame();
                    root.close();
                }
            }

            Button {
                text: "Analyze Position"
                onClicked: {
                    if (root.gameController) {
                        root.gameController.gameMode = 2; // ModeAnalysis
                    }
                    root.close();
                }
            }

            Button {
                text: "Close"
                onClicked: root.close();
            }
        }
    }
}
