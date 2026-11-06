import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuantLens

Rectangle {
    color: Theme.bg

    component Chip: Rectangle {
        id: chip
        property string label
        property bool active: false
        property bool enabledChip: true
        property string tip: ""
        signal clicked
        implicitWidth: Math.max(26, tx.implicitWidth + 14)
        implicitHeight: 24
        radius: 6
        opacity: enabledChip ? 1.0 : 0.35
        color: active ? Theme.accent : (ma.containsMouse && enabledChip ? Theme.panel2 : "transparent")
        border.color: active ? Theme.accent : Theme.border
        Text { id: tx; anchors.centerIn: parent; text: chip.label; font.pixelSize: 12; color: chip.active ? "white" : Theme.muted }
        MouseArea {
            id: ma
            anchors.fill: parent
            hoverEnabled: true
            onClicked: if (chip.enabledChip) chip.clicked()
            ToolTip.visible: containsMouse && chip.tip.length > 0
            ToolTip.text: chip.tip
        }
    }

    component Sep: Rectangle { width: 1; height: 18; color: Theme.border }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: bar.childrenRect.height + 14

            Flow {
                id: bar
                x: 8
                y: 7
                width: parent.width - 16
                spacing: 4

                Repeater {
                    model: [{id: "1s", l: "1s"}, {id: "1m", l: "1m"}, {id: "5m", l: "5m"}, {id: "15m", l: "15m"},
                            {id: "1h", l: "1h"}, {id: "4h", l: "4h"}, {id: "1d", l: "1D"}, {id: "1w", l: "1W"}, {id: "1M", l: "1M"}]
                    Chip {
                        label: modelData.l
                        active: app.interval === modelData.id
                        enabledChip: app.intervals.indexOf(modelData.id) >= 0
                        tip: enabledChip ? "Candle interval" : "Not available for this asset (no tick data from the free source)"
                        onClicked: app.setInterval(modelData.id)
                    }
                }
                Sep {}
                Repeater {
                    model: [{l: "1D", s: 86400}, {l: "5D", s: 5 * 86400}, {l: "1M", s: 30 * 86400}, {l: "3M", s: 91 * 86400},
                            {l: "6M", s: 183 * 86400}, {l: "1Y", s: 365 * 86400}, {l: "5Y", s: 5 * 365 * 86400}, {l: "Max", s: 0}]
                    Chip { label: modelData.l; tip: "Show last " + modelData.l; onClicked: chart.showRange(modelData.s) }
                }
                Sep {}
                Chip { label: "+"; tip: "Zoom in (or mouse wheel)"; onClicked: chart.zoom(1.5) }
                Chip { label: "\u2212"; tip: "Zoom out"; onClicked: chart.zoom(1 / 1.5) }
                Chip { label: "Reset"; tip: "Reset view (or double-click)"; onClicked: chart.resetView() }
                Sep {}
                Chip { label: "Candles"; active: chart.candles; onClicked: chart.candles = !chart.candles }
                Chip { label: "SMA"; active: chart.showSma; onClicked: chart.showSma = !chart.showSma }
                Chip { label: "Bollinger"; active: chart.showBands; onClicked: chart.showBands = !chart.showBands }
                Chip { label: "Volume"; active: chart.showVolume; onClicked: chart.showVolume = !chart.showVolume }
                Chip { label: "RSI"; active: chart.showRsi; onClicked: chart.showRsi = !chart.showRsi }
                Chip { label: "MACD"; active: chart.showMacd; onClicked: chart.showMacd = !chart.showMacd }
                Chip { label: "Log"; active: chart.logScale; tip: "Logarithmic price scale"; onClicked: chart.logScale = !chart.logScale }
                Sep {}
                Chip {
                    label: "Forecast"
                    active: chart.showCone && app.interval === "1d"
                    enabledChip: app.interval === "1d"
                    tip: enabledChip ? "Probability cone" : "Forecast and backtest use daily bars - switch to 1D"
                    onClicked: chart.showCone = !chart.showCone
                }
                Chip { label: "Short"; active: !chart.longCone; visible: chart.showCone && app.interval === "1d"; onClicked: chart.longCone = false }
                Chip { label: "Long"; active: chart.longCone; visible: chart.showCone && app.interval === "1d"; onClicked: chart.longCone = true }
            }
        }

        ChartItem {
            id: chart
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 160
            controller: app

            Text {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: 80
                anchors.bottomMargin: 24
                visible: app.interval !== "1d"
                text: "Forecast / backtest are computed on daily bars"
                color: Theme.muted
                font.pixelSize: 10
            }
            BusyIndicator {
                anchors.centerIn: parent
                running: app.chartLoading
                visible: app.chartLoading
            }
        }
    }
}
