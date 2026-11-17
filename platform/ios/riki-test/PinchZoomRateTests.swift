import MapLibre
import XCTest

/// A pinch recogniser whose state and scale the test writes.
private final class ScriptedPinch: UIPinchGestureRecognizer {
    var scripted: UIGestureRecognizer.State = .possible
    var at: CGPoint = .zero
    var speed: CGFloat = 0

    override var state: UIGestureRecognizer.State {
        get { scripted }
        set { scripted = newValue }
    }

    override var numberOfTouches: Int { 2 }
    override var velocity: CGFloat { speed }

    override func location(in _: UIView?) -> CGPoint { at }
}

final class PinchZoomRateTests: TerrainMapTestCase {
    // 150: by default a pinch that doubles its fingers' spread zooms in one level.
    func testByDefaultAPinchFollowsTheFingers() throws {
        try loadMap()
        XCTAssertEqual(mapView.zoomRate, 1)
        let start = mapView.zoomLevel
        pinch(to: 2)
        XCTAssertEqual(mapView.zoomLevel - start, 1, accuracy: 0.01)
    }

    // 150: Riki Maps found the pinch too quick; at half the rate it zooms half as far.
    func testAGentlerRateZoomsLess() throws {
        try loadMap()
        mapView.zoomRate = 0.5
        let start = mapView.zoomLevel
        pinch(to: 4)
        XCTAssertEqual(mapView.zoomLevel - start, 1, accuracy: 0.01)
    }

    private func pinch(to scale: CGFloat) {
        let gesture = ScriptedPinch()
        gesture.at = CGPoint(x: mapView.bounds.midX, y: mapView.bounds.midY)
        send(gesture, .began, 1)
        send(gesture, .changed, sqrt(scale))
        send(gesture, .changed, scale)
        send(gesture, .ended, scale)
    }

    private func send(_ gesture: ScriptedPinch, _ state: UIGestureRecognizer.State, _ scale: CGFloat) {
        gesture.scripted = state
        gesture.scale = scale
        mapView.perform(NSSelectorFromString("handlePinchGesture:"), with: gesture)
    }
}
