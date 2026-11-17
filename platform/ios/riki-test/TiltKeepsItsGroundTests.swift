import MapLibre
import XCTest

/// Two level fingers whose travel up the screen the test writes.
private final class ScriptedTiltDrag: UIPanGestureRecognizer {
    var scripted: UIGestureRecognizer.State = .possible
    var travel: CGFloat = 0
    var at: CGPoint = .zero

    override var state: UIGestureRecognizer.State {
        get { scripted }
        set { scripted = newValue }
    }

    override var numberOfTouches: Int { 2 }

    override func location(ofTouch touchIndex: Int, in _: UIView?) -> CGPoint {
        CGPoint(x: at.x + (touchIndex == 0 ? -80 : 80), y: at.y - travel)
    }

    override func location(in _: UIView?) -> CGPoint { CGPoint(x: at.x, y: at.y - travel) }

    override func translation(in _: UIView?) -> CGPoint { CGPoint(x: 0, y: -travel) }
}

final class TiltKeepsItsGroundTests: TerrainMapTestCase {
    // 157 relief grown in as the hand tilts keeps the ground in the middle where it was
    func testATiltOverGrowingReliefKeepsTheGroundInTheMiddle() throws {
        try loadMap()
        let style = try XCTUnwrap(mapView.style)
        style.terrainExaggeration = 0
        mapView.isCenterClampedToGround = true
        tilt(0)
        waitUntilFullyRendered()
        let before = groundInTheMiddle()

        let drag = ScriptedTiltDrag()
        drag.at = CGPoint(x: mapView.bounds.midX, y: mapView.bounds.height * 0.7)
        send(drag, .began, travel: 0)
        for step in 1 ... 10 {
            style.terrainExaggeration = Double(step) / 10
            send(drag, .changed, travel: CGFloat(step) * 12)
            waitUntilFullyRendered()
        }
        send(drag, .ended, travel: 120)
        waitUntilFullyRendered()
        waitUntilFullyRendered()

        XCTAssertEqual(mapView.camera.pitch, 60, accuracy: 1)
        XCTAssertEqual(mapView.centerAltitude, Self.groundHeight, accuracy: 5, "the centre stayed at sea level")
        XCTAssertLessThan(distance(before, groundInTheMiddle()), 50, "the ground in the middle slid away")
    }

    // 157 bug: switched on over terrain, the clamp waited for the ground to change
    func testTheClampSwitchedOnPutsTheCentreOnTheGround() throws {
        try loadMap()
        tilt(55)
        waitUntilFullyRendered()
        waitUntilFullyRendered()
        XCTAssertEqual(mapView.centerAltitude, 0, accuracy: 1)
        let corner = CGPoint(x: mapView.bounds.width * 0.2, y: mapView.bounds.height * 0.8)
        let seen = mapView.convert(corner, toCoordinateFrom: mapView)

        mapView.isCenterClampedToGround = true
        waitUntilFullyRendered()

        XCTAssertEqual(mapView.centerAltitude, Self.groundHeight, accuracy: 5)
        XCTAssertLessThan(distance(seen, mapView.convert(corner, toCoordinateFrom: mapView)), 20,
                          "switching the clamp on moved the view")
    }

    private func groundInTheMiddle() -> CLLocationCoordinate2D {
        mapView.convert(CGPoint(x: mapView.bounds.midX, y: mapView.bounds.midY), toCoordinateFrom: mapView)
    }

    private func send(_ drag: ScriptedTiltDrag, _ state: UIGestureRecognizer.State, travel: CGFloat) {
        drag.scripted = state
        drag.travel = travel
        mapView.perform(NSSelectorFromString("handleTwoFingerDragGesture:"), with: drag)
    }

    private func distance(_ one: CLLocationCoordinate2D, _ other: CLLocationCoordinate2D) -> CLLocationDistance {
        CLLocation(latitude: one.latitude, longitude: one.longitude)
            .distance(from: CLLocation(latitude: other.latitude, longitude: other.longitude))
    }
}
