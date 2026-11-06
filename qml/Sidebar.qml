import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root
    color: Theme.panel
    border.color: Theme.border

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Rectangle {
                width: 28; height: 28; radius: 8; color: Theme.accent
                Text { anchors.centerIn: parent; text: "Q"; color: "white"; font.bold: true; font.pixelSize: 16 }
            }
            Text { text: "QuantLens"; color: Theme.text; font.pixelSize: 18; font.bold: true; font.family: Theme.font }
        }

        TextField {
            id: search
            Layout.fillWidth: true
            placeholderText: "Search stocks, ETFs, crypto..."
            color: Theme.text
            placeholderTextColor: Theme.muted
            font.pixelSize: 13
            leftPadding: 12
            background: Rectangle {
                radius: 8
                color: Theme.bg
                border.color: search.activeFocus ? Theme.accent : Theme.border
            }
            onTextChanged: results.model = app.search(text)
            onAccepted: if (results.count > 0) { app.addAsset(results.model[0].symbol); text = "" }
        }

        ListView {
            id: results
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(count, 6) * 40
            visible: search.text.length > 0 && count > 0
            clip: true
            model: []
            delegate: Rectangle {
                width: ListView.view.width
                height: 40
                color: ma.containsMouse ? Theme.panel2 : "transparent"
                radius: 6
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    Text { text: modelData.symbol; color: Theme.text; font.bold: true; font.pixelSize: 13; Layout.preferredWidth: 60 }
                    Text { text: modelData.name; color: Theme.muted; font.pixelSize: 12; elide: Text.ElideRight; Layout.fillWidth: true }
                    Text { text: "+ Add"; color: Theme.accent; font.pixelSize: 12 }
                }
                MouseArea {
                    id: ma
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: { app.addAsset(modelData.symbol); search.text = "" }
                }
            }
        }

        Text { text: "WATCHLIST"; color: Theme.muted; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1 }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 4
            model: app.watchlist
            ScrollBar.vertical: ScrollBar { }

            delegate: Rectangle {
                id: row
                width: ListView.view.width - 8
                height: 56
                radius: 8
                color: app.selected === symbol ? Theme.panel2 : (hover.hovered ? "#172133" : "transparent")
                border.color: app.selected === symbol ? Theme.accent : "transparent"

                // brief green/red flash whenever a new tick arrives
                Rectangle {
                    id: flash
                    anchors.fill: parent
                    radius: 8
                    color: tickDir > 0 ? Theme.up : Theme.down
                    opacity: 0
                }
                NumberAnimation { id: flashAnim; target: flash; property: "opacity"; from: 0.28; to: 0; duration: 450 }
                property int seq: tickSeq
                onSeqChanged: flashAnim.restart()

                HoverHandler { id: hover }
                TapHandler { onTapped: app.select(symbol) }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 8
                    spacing: 8
                    ColumnLayout {
                        Layout.preferredWidth: 62
                        spacing: 1
                        Text { text: symbol; color: Theme.text; font.bold: true; font.pixelSize: 14; font.family: Theme.font }
                        Text { text: status; color: Theme.statusColor(status); font.pixelSize: 9; font.bold: true }
                    }
                    Sparkline {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 26
                        values: spark
                        lineColor: Theme.changeColor(changePct)
                    }
                    ColumnLayout {
                        Layout.preferredWidth: 76
                        spacing: 1
                        Text {
                            Layout.alignment: Qt.AlignRight
                            text: price > 0 ? Theme.fmtPrice(price) : "-"
                            color: Theme.text
                            font.pixelSize: 13
                            font.family: Theme.font
                        }
                        Text {
                            Layout.alignment: Qt.AlignRight
                            text: price > 0 ? Theme.fmtPct(changePct) : ""
                            color: Theme.changeColor(changePct)
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }
                    Text {
                        text: "\u2715"
                        color: Theme.muted
                        font.pixelSize: 11
                        visible: hover.hovered
                        TapHandler { onTapped: app.removeAsset(symbol) }
                    }
                }
            }
        }
    }
}
