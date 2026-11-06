pragma Singleton
import QtQuick

QtObject {
    readonly property color bg: "#0E1621"
    readonly property color panel: "#151F2D"
    readonly property color panel2: "#1A2638"
    readonly property color border: "#223048"
    readonly property color text: "#E6EDF5"
    readonly property color muted: "#8B9BB0"
    readonly property color up: "#26C281"
    readonly property color down: "#F0555A"
    readonly property color accent: "#4DA3FF"
    readonly property color warn: "#F5B84D"
    readonly property string font: "Segoe UI"

    function statusColor(s) {
        if (s === "LIVE") return up
        if (s === "DELAYED") return warn
        if (s === "OFFLINE") return down
        return muted
    }
    function changeColor(v) { return v >= 0 ? up : down }
    function fmtPrice(v) {
        if (v >= 1000) return v.toLocaleString(Qt.locale("en_US"), "f", 2)
        if (v >= 10) return v.toFixed(2)
        return v.toFixed(4)
    }
    function fmtPct(v, digits) {
        return (v >= 0 ? "+" : "") + v.toFixed(digits === undefined ? 2 : digits) + "%"
    }
}
