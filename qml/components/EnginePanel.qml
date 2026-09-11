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
                elide: Text.ElideRight
            }

            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: (root.uciController && root.uciController.isAnalyzing) ? "#22c55e" : "#64748b"
            }

            Button {
                text: (root.uciController && root.uciController.isAnalyzing) ? "Stop" : "Analyze"
                font.pixelSize: 11
                Layout.preferredHeight: 26
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

        // Stats: Depth, NPS, MultiPV selector
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "D: " + (root.uciController ? root.uciController.depth : 0)
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

            Text {
                text: "Lines:"
                font.pixelSize: 10
                color: "#888888"
            }

            Row {
                spacing: 2
                Repeater {
                    model: [1, 2, 3]
                    delegate: Rectangle {
                        required property int modelData
                        width: 20
                        height: 20
                        radius: 3
                        color: (root.uciController && root.uciController.multiPv === modelData) ? "#3b82f6" : "#33322e"

                        Text {
                            anchors.centerIn: parent
                            text: modelData.toString()
                            font.pixelSize: 10
                            font.bold: true
                            color: "#ffffff"
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                if (root.uciController) {
                                    root.uciController.multiPv = modelData;
                                    if (root.uciController.isAnalyzing) {
                                        root.uciController.startInfiniteAnalysis();
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Candidate Lines (MultiPV) List
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 65
            color: "#1a1917"
            radius: 4
            clip: true

            ListView {
                id: linesView
                anchors.fill: parent
                anchors.margins: 4
                spacing: 3
                model: root.uciController ? root.uciController.candidateLines : []

                delegate: Rectangle {
                    id: lineDelegate
                    required property var modelData
                    width: linesView.width
                    height: 22
                    color: (modelData.rank === 1) ? "#2b2a27" : "#22211e"
                    radius: 3

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 4
                        anchors.rightMargin: 4
                        spacing: 6

                        Text {
                            text: "#" + modelData.rank
                            font.pixelSize: 10
                            font.bold: true
                            color: "#94a3b8"
                            Layout.preferredWidth: 16
                        }

                        Rectangle {
                            Layout.preferredWidth: 44
                            Layout.preferredHeight: 16
                            radius: 3
                            color: {
                                var ev = modelData.eval;
                                if (modelData.isMate) {
                                    return modelData.mateIn > 0 ? "#15803d" : "#b91c1c";
                                }
                                if (ev > 0.5) return "#15803d";
                                if (ev < -0.5) return "#b91c1c";
                                return "#475569";
                            }

                            Text {
                                anchors.centerIn: parent
                                text: {
                                    if (modelData.isMate) {
                                        return modelData.mateIn > 0 ? "+M" + modelData.mateIn : "-M" + (-modelData.mateIn);
                                    }
                                    var ev = modelData.eval;
                                    return (ev >= 0 ? "+" : "") + Number(ev).toFixed(2);
                                }
                                font.pixelSize: 9
                                font.bold: true
                                color: "#ffffff"
                            }
                        }

                        Text {
                            text: modelData.pv ? modelData.pv : ""
                            font.pixelSize: 10
                            font.family: "Monospace"
                            color: "#cbd5e1"
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                text: (root.uciController && root.uciController.isAnalyzing) ? "Analyzing position..." : "Engine idle"
                font.pixelSize: 11
                color: "#64748b"
                visible: linesView.count === 0
            }
        }

        // AI Strength / Elo setting (for PvAI mode)
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
