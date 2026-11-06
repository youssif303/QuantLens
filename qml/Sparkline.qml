import QtQuick
import QtQuick.Shapes

Item {
    id: root
    property var values: []
    property color lineColor: Theme.up

    readonly property var points: {
        const v = values
        if (!v || v.length < 2) return []
        let lo = v[0], hi = v[0]
        for (let i = 1; i < v.length; ++i) { lo = Math.min(lo, v[i]); hi = Math.max(hi, v[i]) }
        const r = Math.max(hi - lo, 1e-12)
        const out = []
        for (let i = 0; i < v.length; ++i)
            out.push(Qt.point(i / (v.length - 1) * width, height - 2 - (v[i] - lo) / r * (height - 4)))
        return out
    }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: root.lineColor
            strokeWidth: 1.5
            fillColor: "transparent"
            joinStyle: ShapePath.RoundJoin
            PathPolyline { path: root.points }
        }
    }
}
