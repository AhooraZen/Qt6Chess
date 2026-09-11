import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root
    property var gameController: null
    property string playerColor: gameController ? gameController.activeColor : "white"

    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape

    onClosed: {
        if (root.gameController && root.gameController.isPromotionPending) {
            root.gameController.cancelPromotion();
        }
    }

    background: Rectangle {
        color: "#262522"
        radius: 8
        border.color: "#486581"
        border.width: 2
    }

    contentItem: ColumnLayout {
        spacing: 12

        Text {
            text: "Promote Pawn"
            font.bold: true
            font.pixelSize: 14
            color: "#ffffff"
            Layout.alignment: Qt.AlignHCenter
        }

        RowLayout {
            spacing: 8

            Repeater {
                model: [
                    { type: "q", label: "Queen" },
                    { type: "r", label: "Rook" },
                    { type: "b", label: "Bishop" },
                    { type: "n", label: "Knight" }
                ]

                delegate: Rectangle {
                    required property var modelData
                    width: 64
                    height: 64
                    radius: 6
                    color: pMouse.containsMouse ? "#3e3d38" : "#302e2b"

                    Image {
                        anchors.centerIn: parent
                        width: 48
                        height: 48
                        sourceSize.width: 96
                        sourceSize.height: 96
                        fillMode: Image.PreserveAspectFit
                        source: {
                            var prefix = (root.playerColor === "black") ? "b" : "w";
                            return "qrc:/qt/qml/Qt6Chess/UI/resources/pieces/cburnett/" + prefix + modelData.type.toUpperCase() + ".svg";
                        }
                    }

                    MouseArea {
                        id: pMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            if (root.gameController) {
                                root.gameController.submitPromotion(modelData.type);
                            }
                            root.close();
                        }
                    }
                }
            }
        }

        Button {
            text: "Cancel"
            Layout.alignment: Qt.AlignHCenter
            onClicked: {
                if (root.gameController) root.gameController.cancelPromotion();
                root.close();
            }
        }
    }
}
