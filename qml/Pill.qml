import QtQuick

Rectangle {
    id: root
    property string label: ""
    property color tone: Theme.muted
    implicitWidth: t.implicitWidth + 14
    implicitHeight: 20
    radius: 10
    color: Qt.rgba(tone.r, tone.g, tone.b, 0.16)
    border.color: Qt.rgba(tone.r, tone.g, tone.b, 0.5)
    Text {
        id: t
        anchors.centerIn: parent
        text: root.label
        color: root.tone
        font.pixelSize: 11
        font.bold: true
        font.family: Theme.font
    }
}
