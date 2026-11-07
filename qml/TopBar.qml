import QtQuick
import QtQuick.Layouts

Rectangle {
    color: Theme.panel
    border.color: Theme.border

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        spacing: 18

        ColumnLayout {
            spacing: 0
            Text { text: app.selected; color: Theme.text; font.pixelSize: 22; font.bold: true; font.family: Theme.font }
            Text { text: app.name; color: Theme.muted; font.pixelSize: 12 }
        }

        Text {
            text: app.price > 0 ? (app.selected.charAt(0) === "^" ? "" : (app.currency === "EUR" ? "\u20AC" : "$")) + Theme.fmtPrice(app.price * app.displayFactor) : "-"
            color: Theme.text
            font.pixelSize: 30
            font.bold: true
            font.family: Theme.font
        }

        Pill {
            label: Theme.fmtPct(app.changePct)
            tone: Theme.changeColor(app.changePct)
            visible: app.price > 0
        }

        Item { Layout.fillWidth: true }

        Row {
            spacing: 0
            Layout.alignment: Qt.AlignVCenter
            Repeater {
                model: ["USD", "EUR"]
                Rectangle {
                    width: 46; height: 26
                    radius: 6
                    color: app.currency === modelData ? Theme.accent : "transparent"
                    border.color: app.currency === modelData ? Theme.accent : Theme.border
                    Text { anchors.centerIn: parent; text: modelData === "EUR" ? "\u20AC EUR" : "$ USD"; font.pixelSize: 11; color: app.currency === modelData ? "white" : Theme.muted }
                    MouseArea { anchors.fill: parent; onClicked: app.setCurrency(modelData) }
                }
            }
        }

        ColumnLayout {
            spacing: 2
            Layout.alignment: Qt.AlignVCenter
            Pill {
                Layout.alignment: Qt.AlignRight
                label: (app.status === "LIVE" ? "\u25CF " : "") + app.status
                tone: Theme.statusColor(app.status)
            }
            Text {
                Layout.alignment: Qt.AlignRight
                color: Theme.muted
                font.pixelSize: 11
                text: {
                    if (app.status === "LIVE") return "Real-time stream, updated " + app.lastUpdate
                    if (app.status === "DELAYED") return "Polled quote (may be delayed), updated " + app.lastUpdate
                    if (app.status === "OFFLINE") return "Offline demo data (no live connection)"
                    return "Daily history - waiting for live quote"
                }
            }
        }
    }
}
