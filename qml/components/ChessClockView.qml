import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    property var clock: null
    property bool isWhiteSide: true
    property string playerName: isWhiteSide ? "White" : "Black"
    property bool isActive: clock ? (clock.activeSide === (isWhiteSide ? 1 : 2) && clock.isRunning) : false

    implicitHeight: 44
    implicitWidth: 300
    Layout.fillWidth: true
    Layout.preferredHeight: 44
    color: "#262522"
    radius: 6
    border.color: isActive ? "#38bdf8" : "#383734"
    border.width: isActive ? 2 : 1

    RowLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        // Color marker
        Rectangle {
            width: 16
            height: 16
            radius: 8
            color: root.isWhiteSide ? "#ffffff" : "#2b2b2b"
            border.color: "#888888"
            border.width: 1
        }

        // Name
        Text {
            text: root.playerName
            font.bold: true
            font.pixelSize: 13
            color: "#e0e0e0"
            Layout.fillWidth: true
        }

        // Clock Time
        Rectangle {
            Layout.preferredWidth: 84
            Layout.fillHeight: true
            radius: 4
            color: root.isActive ? "#1b2838" : "#1e1e1c"

            Text {
                anchors.centerIn: parent
                font.family: "Monospace"
                font.pixelSize: 16
                font.bold: true
                color: {
                    if (root.clock && (root.isWhiteSide ? root.clock.whiteTimeMs : root.clock.blackTimeMs) < 20000 && !root.clock.isUnlimited) {
                        return "#f87171"; // Red alert low time
                    }
                    return root.isActive ? "#38bdf8" : "#ffffff";
                }
                text: root.clock ? (root.isWhiteSide ? root.clock.whiteTimeString : root.clock.blackTimeString) : "--:--"
            }
        }
    }
}
