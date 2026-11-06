import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QuantLens

Rectangle {
    color: Theme.bg

    property int bars: 180

    component Chip: Rectangle {
        id: chip
        property string label
        property bool active: false
        signal clicked
        implicitWidth: tx.implicitWidth + 14
        implicitHeight: 24
        radius: 6
        color: active ? Theme.accent : (ma.containsMouse ? Theme.panel2 : "transparent")
        border.color: active ? Theme.accent : Theme.border
        Text { id: tx; anchors.centerIn: parent; text: chip.label; font.pixelSize: 12; color: chip.active ? "white" : Theme.muted }
        MouseArea { id: ma; anchors.fill: parent; hoverEnabled: true; onClicked: chip.clicked() }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 8
            spacing: 4
            Repeater {
                model: [{l: "1M", b: 30}, {l: "3M", b: 90}, {l: "6M", b: 180}, {l: "1Y", b: 365}, {l: "5Y", b: 1825}]
                Chip { label: modelData.l; active: chart.bars === modelData.b; onClicked: chart.bars = modelData.b }
            }
            Item { width: 6 }
            Chip { label: "Candles"; active: chart.candles; onClicked: chart.candles = !chart.candles }
            Chip { label: "SMA"; active: chart.showSma; onClicked: chart.showSma = !chart.showSma }
            Chip { label: "Bollinger"; active: chart.showBands; onClicked: chart.showBands = !chart.showBands }
            Chip { label: "RSI"; active: chart.showRsi; onClicked: chart.showRsi = !chart.showRsi }
            Item { Layout.fillWidth: true }
            Chip { label: "Forecast"; active: chart.showCone; onClicked: chart.showCone = !chart.showCone }
            Chip { label: "Short"; active: !chart.longCone; visible: chart.showCone; onClicked: chart.longCone = false }
            Chip { label: "Long"; active: chart.longCone; visible: chart.showCone; onClicked: chart.longCone = true }
        }

        ChartItem {
            id: chart
            Layout.fillWidth: true
            Layout.fillHeight: true
            controller: app
            bars: 180
        }
    }
}
