import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    property string title: ""
    property var out: ({})
    readonly property bool ok: out && out.valid === true
    readonly property real probUp: ok ? out.probUp : 0.5
    readonly property bool isUp: ok && out.direction === "Up"
    readonly property bool isDown: ok && out.direction === "Down"
    readonly property color tone: isUp ? Theme.up : (isDown ? Theme.down : Theme.warn)

    color: Theme.panel
    radius: 12
    border.color: Theme.border
    implicitHeight: col.implicitHeight + 28

    ColumnLayout {
        id: col
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Text { text: root.title; color: Theme.text; font.bold: true; font.pixelSize: 14; font.family: Theme.font }
            Item { Layout.fillWidth: true }
            Text { text: ok ? out.label : ""; color: Theme.muted; font.pixelSize: 11 }
        }

        Text {
            visible: !root.ok
            text: "Not enough history for a forecast"
            color: Theme.muted
            font.pixelSize: 12
        }

        RowLayout {
            visible: root.ok
            Layout.fillWidth: true
            spacing: 12
            Text {
                text: root.isUp ? "\u25B2" : (root.isDown ? "\u25BC" : "\u25C6")
                color: root.tone
                font.pixelSize: 30
            }
            ColumnLayout {
                spacing: 0
                Text {
                    text: root.ok ? (root.isUp ? "Up " + Math.round(root.probUp * 100) + "%"
                                  : (root.isDown ? "Down " + Math.round((1 - root.probUp) * 100) + "%" : "Neutral")) : ""
                    color: root.tone
                    font.pixelSize: 24
                    font.bold: true
                    font.family: Theme.font
                }
                Text {
                    text: root.ok ? (root.isUp ? "Bullish lean" : (root.isDown ? "Bearish lean"
                          : "Up " + Math.round(root.probUp * 100) + "% / Down " + Math.round((1 - root.probUp) * 100) + "%")) : ""
                    color: Theme.muted
                    font.pixelSize: 11
                }
            }
            Item { Layout.fillWidth: true }
            Pill {
                visible: root.ok
                label: root.ok ? out.confidence + " confidence" : ""
                tone: root.ok && out.confidence === "High" ? Theme.up : (root.ok && out.confidence === "Medium" ? Theme.warn : Theme.muted)
            }
        }

        // probability bar: down | up
        Rectangle {
            visible: root.ok
            Layout.fillWidth: true
            height: 6
            radius: 3
            color: Theme.down
            Rectangle {
                width: parent.width * root.probUp
                height: parent.height
                radius: 3
                color: Theme.up
            }
        }

        RowLayout {
            visible: root.ok
            Layout.fillWidth: true
            ColumnLayout {
                spacing: 0
                Text { text: "Expected move"; color: Theme.muted; font.pixelSize: 11 }
                Text {
                    text: root.ok ? Theme.fmtPct(out.expectedReturn * 100, 1) : ""
                    color: root.ok ? Theme.changeColor(out.expectedReturn) : Theme.text
                    font.pixelSize: 15
                    font.bold: true
                }
            }
            Item { Layout.fillWidth: true }
            ColumnLayout {
                spacing: 0
                Text { text: "Likely range (80%)"; color: Theme.muted; font.pixelSize: 11; Layout.alignment: Qt.AlignRight }
                Text {
                    text: root.ok ? Theme.fmtPct(out.low * 100, 1) + "  to  " + Theme.fmtPct(out.high * 100, 1) : ""
                    color: Theme.text
                    font.pixelSize: 13
                    Layout.alignment: Qt.AlignRight
                }
            }
        }

        Repeater {
            model: root.ok ? out.reasons : []
            delegate: Text {
                Layout.fillWidth: true
                text: "\u2022 " + modelData
                color: Theme.muted
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
        }
    }
}
