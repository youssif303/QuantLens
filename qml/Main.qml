import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1440
    height: 880
    minimumWidth: 1280
    minimumHeight: 700
    visible: true
    title: "QuantLens - " + app.selected
    color: Theme.bg

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Sidebar {
                Layout.preferredWidth: 280
                Layout.fillHeight: true
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0
                TopBar { Layout.fillWidth: true; Layout.preferredHeight: 76 }
                ChartPanel { Layout.fillWidth: true; Layout.fillHeight: true }
            }

            Rectangle {
                Layout.preferredWidth: 340
                Layout.fillHeight: true
                color: Theme.bg
                border.color: Theme.border

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 12
                    contentWidth: availableWidth
                    clip: true

                    ColumnLayout {
                        width: parent.width
                        spacing: 12
                        Text { text: "OUTLOOK"; color: Theme.muted; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1 }
                        ForecastCard { Layout.fillWidth: true; title: "Short term"; out: app.shortOutlook }
                        ForecastCard { Layout.fillWidth: true; title: "Long term"; out: app.longOutlook }
                        BacktestCard { Layout.fillWidth: true; shortBt: app.shortBacktest; longBt: app.longBacktest }
                        BusyIndicator { Layout.alignment: Qt.AlignHCenter; running: app.busy; visible: app.busy }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            color: Theme.panel
            border.color: Theme.border
            Text {
                anchors.centerIn: parent
                text: "Analysis only - not financial advice. Statistical estimates, no guarantee of future results.   Data: Binance, Yahoo Finance."
                color: Theme.muted
                font.pixelSize: 11
            }
        }
    }
}
