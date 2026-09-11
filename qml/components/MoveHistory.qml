import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property var gameController: null

    color: "#262522"
    radius: 6

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // Header
        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "Move History"
                font.bold: true
                font.pixelSize: 13
                color: "#e0e0e0"
                Layout.fillWidth: true
            }

            Button {
                text: "Copy PGN"
                font.pixelSize: 10
                flat: true
                onClicked: {
                    if (root.gameController) root.gameController.copyPgnToClipboard();
                }
            }

            Button {
                text: "Load PGN"
                font.pixelSize: 10
                flat: true
                onClicked: pgnDialog.open()
            }
        }

        // Moves list
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#1e1e1c"
            radius: 4
            clip: true

            ListView {
                id: moveList
                anchors.fill: parent
                model: root.gameController ? root.gameController.historyModel : null
                spacing: 2

                delegate: Rectangle {
                    id: rowDelegate
                    required property int turnNumber
                    required property string whiteSan
                    required property string blackSan
                    required property bool isWhiteSelected
                    required property bool isBlackSelected

                    width: moveList.width
                    height: 24
                    color: (turnNumber % 2 === 0) ? "#242320" : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 4

                        Text {
                            text: turnNumber + "."
                            color: "#888888"
                            font.pixelSize: 11
                            Layout.preferredWidth: 26
                        }

                        // White move button
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: isWhiteSelected ? "#486581" : "transparent"
                            radius: 3

                            Text {
                                anchors.centerIn: parent
                                text: whiteSan
                                font.bold: isWhiteSelected
                                font.pixelSize: 12
                                color: isWhiteSelected ? "#ffffff" : "#d0d0d0"
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (root.gameController) {
                                        root.gameController.goToPly(rowDelegate.turnNumber * 2 - 1);
                                    }
                                }
                            }
                        }

                        // Black move button
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: isBlackSelected ? "#486581" : "transparent"
                            radius: 3
                            visible: blackSan.length > 0

                            Text {
                                anchors.centerIn: parent
                                text: blackSan
                                font.bold: isBlackSelected
                                font.pixelSize: 12
                                color: isBlackSelected ? "#ffffff" : "#d0d0d0"
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (root.gameController) {
                                        root.gameController.goToPly(rowDelegate.turnNumber * 2);
                                    }
                                }
                            }
                        }
                    }
                }

                onCountChanged: {
                    positionViewAtEnd();
                }
            }
        }

        // Navigation Bar (<, >, <<, >>)
        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            Button {
                text: "⏮"
                Layout.fillWidth: true
                onClicked: if (root.gameController) root.gameController.goToPly(0);
            }
            Button {
                text: "◀"
                Layout.fillWidth: true
                onClicked: {
                    if (root.gameController) {
                        var ply = root.gameController.historyModel.currentPly;
                        root.gameController.goToPly(Math.max(0, ply - 1));
                    }
                }
            }
            Button {
                text: "▶"
                Layout.fillWidth: true
                onClicked: {
                    if (root.gameController) {
                        var p = root.gameController.historyModel.currentPly;
                        var total = root.gameController.historyModel.totalPly;
                        root.gameController.goToPly(Math.min(total, p + 1));
                    }
                }
            }
            Button {
                text: "⏭"
                Layout.fillWidth: true
                onClicked: {
                    if (root.gameController) {
                        var tot = root.gameController.historyModel.totalPly;
                        root.gameController.goToPly(tot);
                    }
                }
            }
        }
    }

    Dialog {
        id: pgnDialog
        title: "Load PGN Game"
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        ColumnLayout {
            spacing: 8
            width: 320

            Text {
                text: "Paste PGN text:"
                color: "#d0d0d0"
                font.pixelSize: 11
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: 120

                TextArea {
                    id: pgnInput
                    placeholderText: "1. e4 e5 2. Nf3 Nc6..."
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                }
            }
        }

        onAccepted: {
            if (root.gameController && pgnInput.text.trim().length > 0) {
                root.gameController.loadPgn(pgnInput.text.trim());
            }
        }
        onOpened: {
            pgnInput.text = "";
            pgnInput.forceActiveFocus();
        }
    }
}
