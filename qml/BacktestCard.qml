import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    property var shortBt: ({})
    property var longBt: ({})
    property bool showLong: false
    readonly property var bt: showLong ? longBt : shortBt
    readonly property bool ok: bt && bt.valid === true

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
            Text { text: "Track record (backtest)"; color: Theme.text; font.bold: true; font.pixelSize: 14 }
            Item { Layout.fillWidth: true }
            Repeater {
                model: ["Short", "Long"]
                delegate: Text {
                    text: modelData
                    color: (index === 1) === root.showLong ? Theme.accent : Theme.muted
                    font.pixelSize: 12
                    font.bold: (index === 1) === root.showLong
                    leftPadding: 6
                    TapHandler { onTapped: root.showLong = (index === 1) }
                }
            }
        }

        Text {
            visible: !root.ok
            text: "Not enough history for a backtest"
            color: Theme.muted
            font.pixelSize: 12
        }

        GridLayout {
            visible: root.ok
            columns: 3
            Layout.fillWidth: true
            columnSpacing: 10
            Repeater {
                model: root.ok ? [
                    {k: "Hit rate", v: (bt.hitRate * 100).toFixed(1) + "%", s: "always-up: " + (bt.baseline * 100).toFixed(1) + "%"},
                    {k: "Brier score", v: bt.brier.toFixed(3), s: "baseline: " + bt.brierBase.toFixed(3)},
                    {k: "Skill", v: (bt.skill >= 0 ? "+" : "") + (bt.skill * 100).toFixed(1) + "%", s: bt.samples + " forecasts"}
                ] : []
                delegate: ColumnLayout {
                    spacing: 0
                    Text { text: modelData.k; color: Theme.muted; font.pixelSize: 11 }
                    Text {
                        text: modelData.v
                        color: modelData.k === "Skill" ? (root.bt.skill > 0.005 ? Theme.up : Theme.warn) : Theme.text
                        font.pixelSize: 17
                        font.bold: true
                    }
                    Text { text: modelData.s; color: Theme.muted; font.pixelSize: 10 }
                }
            }
        }

        Text {
            visible: root.ok
            text: "Calibration: predicted vs. actual share of up-moves"
            color: Theme.muted
            font.pixelSize: 11
        }

        Row {
            visible: root.ok
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: root.ok ? root.bt.bins : []
                delegate: Item {
                    width: (col.width - 24) / 5
                    height: 58
                    visible: true
                    Rectangle {
                        anchors.bottom: lbl.top
                        anchors.bottomMargin: 2
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: parent.width * 0.7
                        height: modelData.n > 0 ? Math.max(2, modelData.o * 40) : 0
                        radius: 2
                        color: Theme.accent
                    }
                    Rectangle { // predicted marker
                        visible: modelData.n > 0
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: parent.width * 0.9
                        height: 2
                        color: Theme.warn
                        y: parent.height - 16 - modelData.p * 40
                    }
                    Text {
                        id: lbl
                        anchors.bottom: parent.bottom
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData.n > 0 ? modelData.n + "x" : "-"
                        color: Theme.muted
                        font.pixelSize: 10
                    }
                }
            }
        }

        Text {
            visible: root.ok
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.muted
            font.pixelSize: 10
            text: "Bars: actual share of up-moves per probability bucket; orange line: model's mean prediction. Walk-forward, no look-ahead. Positive skill = better than guessing the base rate."
        }
    }
}
