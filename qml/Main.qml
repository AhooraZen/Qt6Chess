import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "board"
import "components"
import "dialogs"

ApplicationWindow {
    id: window
    visible: true
    width: 1080
    height: 720
    minimumWidth: 800
    minimumHeight: 600
    title: "Qt6Chess - " + (gameController ? gameController.statusText : "Desktop Chess")
    color: "#181715"

    // Shortcuts
    Shortcut {
        sequence: "F"
        onActivated: if (gameController) gameController.flipBoard()
    }
    Shortcut {
        sequence: "Left"
        onActivated: {
            if (gameController) {
                var p = gameController.historyModel.currentPly;
                gameController.goToPly(Math.max(0, p - 1));
            }
        }
    }
    Shortcut {
        sequence: "Right"
        onActivated: {
            if (gameController) {
                var ply = gameController.historyModel.currentPly;
                var tot = gameController.historyModel.totalPly;
                gameController.goToPly(Math.min(tot, ply + 1));
            }
        }
    }
    Shortcut {
        sequence: "StandardKey.Undo"
        onActivated: if (gameController) gameController.undoMove()
    }
    Shortcut {
        sequence: "StandardKey.New"
        onActivated: if (gameController) gameController.newGame()
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        // ================= Left Sidebar: Controls & Modes =================
        Rectangle {
            Layout.preferredWidth: 200
            Layout.fillHeight: true
            color: "#21201d"
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                Text {
                    text: "Qt6Chess"
                    font.bold: true
                    font.pixelSize: 18
                    color: "#38bdf8"
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#33322e"
                }

                Text {
                    text: "Mode"
                    font.bold: true
                    font.pixelSize: 12
                    color: "#a0a0a0"
                }

                ComboBox {
                    id: modeCombo
                    Layout.fillWidth: true
                    model: ["Play vs Computer", "Pass & Play", "Analysis Board"]
                    currentIndex: gameController ? gameController.gameMode : 0
                    onActivated: (index) => {
                        if (gameController) gameController.gameMode = index;
                    }
                }

                Text {
                    text: "Play As"
                    font.bold: true
                    font.pixelSize: 12
                    color: "#a0a0a0"
                    visible: modeCombo.currentIndex === 0
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: modeCombo.currentIndex === 0
                    spacing: 8

                    Button {
                        text: "White"
                        Layout.fillWidth: true
                        highlighted: gameController && gameController.playerColor === 1
                        onClicked: if (gameController) gameController.playerColor = 1;
                    }

                    Button {
                        text: "Black"
                        Layout.fillWidth: true
                        highlighted: gameController && gameController.playerColor === 2
                        onClicked: if (gameController) gameController.playerColor = 2;
                    }
                }

                Text {
                    text: "Time Control"
                    font.bold: true
                    font.pixelSize: 12
                    color: "#a0a0a0"
                }

                ComboBox {
                    id: timeCombo
                    Layout.fillWidth: true
                    model: ["Blitz 3+2", "Rapid 10+0", "Bullet 1+1", "Classical 15+10", "Unlimited"]
                    currentIndex: 0
                }

                Text {
                    text: "Theme:"
                    font.bold: true
                    font.pixelSize: 11
                    color: "#a0a0a0"
                }

                ComboBox {
                    id: themeCombo
                    Layout.fillWidth: true
                    model: ["Emerald", "Wood", "Slate", "Midnight"]
                    currentIndex: 0
                }

                Button {
                    text: "New Game"
                    Layout.fillWidth: true
                    highlighted: true
                    onClicked: {
                        if (!gameController) return;
                        var tMin = 3; var inc = 2;
                        if (timeCombo.currentIndex === 1) { tMin = 10; inc = 0; }
                        else if (timeCombo.currentIndex === 2) { tMin = 1; inc = 1; }
                        else if (timeCombo.currentIndex === 3) { tMin = 15; inc = 10; }
                        else if (timeCombo.currentIndex === 4) { tMin = 0; inc = 0; }

                        gameController.newGame(modeCombo.currentIndex, gameController.playerColor, tMin, inc);
                    }
                }

                Button {
                    text: "Flip Board (F)"
                    Layout.fillWidth: true
                    onClicked: if (gameController) gameController.flipBoard();
                }

                Button {
                    text: "Undo Move (Ctrl+Z)"
                    Layout.fillWidth: true
                    onClicked: if (gameController) gameController.undoMove();
                }

                Button {
                    text: "Resign"
                    Layout.fillWidth: true
                    onClicked: if (gameController) gameController.resignCurrentPlayer();
                }

                Item { Layout.fillHeight: true }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#33322e"
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Button {
                        text: "Copy FEN"
                        Layout.fillWidth: true
                        onClicked: if (gameController) gameController.copyFenToClipboard();
                    }

                    Button {
                        text: "Paste FEN"
                        Layout.fillWidth: true
                        onClicked: fenDialog.open()
                    }
                }

                Button {
                    text: "Sound: " + (gameController && gameController.soundManager.soundEnabled ? "ON" : "OFF")
                    Layout.fillWidth: true
                    onClicked: {
                        if (gameController) {
                            gameController.soundManager.soundEnabled = !gameController.soundManager.soundEnabled;
                        }
                    }
                }
            }
        }

        // ================= Center Column: Clocks + Board + EvalBar =================
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 500
            spacing: 12

            // Vertical Evaluation Bar aligned with Board
            EvalBar {
                id: evalBar
                Layout.fillHeight: true
                Layout.preferredWidth: 26
                Layout.topMargin: 50
                Layout.bottomMargin: 50
                uciController: gameController ? gameController.uciController : null
                flipped: gameController ? gameController.flipped : false
            }

            // Board Container with Clocks
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8

                // Top Clock
                ChessClockView {
                    Layout.fillWidth: true
                    clock: gameController ? gameController.clock : null
                    isWhiteSide: gameController ? gameController.flipped : false
                    playerName: (gameController && gameController.gameMode === 0)
                        ? (isWhiteSide ? "White (Stockfish)" : "Black (Stockfish)")
                        : (isWhiteSide ? "White" : "Black")
                }

                // Interactive Board
                Item {
                    id: boardArea
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 380

                    ChessBoard {
                        id: board
                        anchors.centerIn: parent
                        width: Math.max(300, Math.min(boardArea.width, boardArea.height) - 12)
                        height: width
                        gameController: gameController
                        boardTheme: themeCombo.currentIndex
                    }
                }

                // Bottom Clock
                ChessClockView {
                    Layout.fillWidth: true
                    clock: gameController ? gameController.clock : null
                    isWhiteSide: gameController ? !gameController.flipped : true
                    playerName: (gameController && gameController.gameMode === 0)
                        ? (isWhiteSide ? "White (You)" : "Black (You)")
                        : (isWhiteSide ? "White" : "Black")
                }
            }
        }

        // ================= Right Column: Engine & Move History =================
        ColumnLayout {
            Layout.preferredWidth: 290
            Layout.minimumWidth: 260
            Layout.maximumWidth: 320
            Layout.fillHeight: true
            spacing: 8

            // Status Badge
            Rectangle {
                Layout.fillWidth: true
                height: 38
                color: "#262522"
                radius: 6

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8

                    Text {
                        text: gameController ? gameController.statusText : ""
                        font.bold: true
                        font.pixelSize: 13
                        color: {
                            if (!gameController) return "#ffffff";
                            if (gameController.isGameOver) return "#f87171";
                            return "#e2e8f0";
                        }
                        Layout.fillWidth: true
                    }

                    BusyIndicator {
                        running: gameController ? gameController.isThinking : false
                        Layout.preferredWidth: 22
                        Layout.preferredHeight: 22
                    }
                }
            }

            // Move History
            MoveHistory {
                Layout.fillWidth: true
                Layout.fillHeight: true
                gameController: gameController
            }

            // Engine Analysis Panel
            EnginePanel {
                Layout.fillWidth: true
                Layout.preferredHeight: 180
                Layout.minimumHeight: 140
                uciController: gameController ? gameController.uciController : null
                gameController: gameController
            }
        }
    }

    // Dialogs
    PromotionDialog {
        id: promoDialog
        gameController: gameController
    }

    GameOverDialog {
        id: gameOverDialog
        gameController: gameController
    }

    Dialog {
        id: fenDialog
        title: "Load FEN Position"
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        ColumnLayout {
            spacing: 8
            width: 380

            Text {
                text: "Paste FEN string:"
                color: "#d0d0d0"
                font.pixelSize: 11
            }

            TextField {
                id: fenInput
                Layout.fillWidth: true
                placeholderText: "e.g. rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
                selectByMouse: true
            }
        }

        onAccepted: {
            if (gameController && fenInput.text.trim().length > 0) {
                gameController.loadFen(fenInput.text.trim());
            }
        }
        onOpened: {
            fenInput.text = "";
            fenInput.forceActiveFocus();
        }
    }

    Connections {
        target: gameController
        function onPromotionPendingChanged() {
            if (gameController && gameController.isPromotionPending) {
                promoDialog.open();
            } else {
                promoDialog.close();
            }
        }
        function onGameOverChanged() {
            if (gameController && gameController.isGameOver) {
                gameOverDialog.open();
            }
        }
    }
}
