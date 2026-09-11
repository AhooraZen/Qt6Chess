import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property var uciController: null
    property var gameController: null

    color: "#262522"
    radius: 6

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // Top Engine Status
        RowLayout {
            Layout.fillWidth: true

            Text {
                text: root.uciController ? root.uciController.engineName : "Engine"
                font.bold: true
                font.pixelSize: 13
                color: "#e0e0e0"
                Layout.fillWidth: true
            }

            Rectangle {
                width: 10
                height: 10
                radius: 5
                color: (root.uciController && root.uciController.isAnalyzing) ? "#22c55e" : "#64748b"
            }

            Button {
                text: (root.uciController && root.uciController.isAnalyzing) ? "Stop" : "Analyze"
                font.pixelSize: 11
                onClicked: {
                    if (!root.uciController) return;
                    if (root.uciController.isAnalyzing) {
                        root.uciController.stopAnalysis();
                    } else {
                        root.uciController.startInfiniteAnalysis();
                    }
                }
            }
        }

        // Stats: Depth, NPS
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Text {
                text: "Depth: " + (root.uciController ? root.uciController.depth : 0)
                font.pixelSize: 11
                color: "#a0a0a0"
            }

            Text {
                text: {
                    var nps = root.uciController ? root.uciController.nps : 0;
                    if (nps > 1000000) return (nps / 1000000).toFixed(1) + " MN/s";
                    if (nps > 1000) return (nps / 1000).toFixed(0) + " kN/s";
                    return nps + " N/s";
                }
                font.pixelSize: 11
                color: "#a0a0a0"
                Layout.fillWidth: true
            }
        }

        // AI Strength / Elo setting
        RowLayout {
            Layout.fillWidth: true
            visible: Boolean(root.gameController && root.gameController.gameMode === 0) // PvAI

            Text {
                text: "AI Elo: " + (root.gameController ? root.gameController.aiElo : 1500)
                font.pixelSize: 11
                color: "#d0d0d0"
                Layout.preferredWidth: 80
            }

            Slider {
                Layout.fillWidth: true
                from: 800
                to: 3000
                stepSize: 100
                value: root.gameController ? root.gameController.aiElo : 1500
                onMoved: {
                    if (root.gameController) {
                        root.gameController.aiElo = Math.round(value);
                    }
                }
            }
        }
    }
}
