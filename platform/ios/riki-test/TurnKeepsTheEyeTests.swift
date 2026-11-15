import MapLibre
import XCTest

/// A rotation recogniser whose state and angle the test writes.
private final class ScriptedTurn: UIRotationGestureRecognizer {
    var scripted: UIGestureRecognizer.State = .possible
    var at: CGPoint = .zero

    override var state: UIGestureRecognizer.State {
        get { scripted }
        set { scripted = newValue }
    }

    override var numberOfTouches: Int { 2 }

    override func location(in _: UIView?) -> CGPoint { at }
}

final class TurnKeepsTheEyeTests: TerrainMapTestCase {
    // 138 bug: tilted, a turn swung the camera round the ground ahead, kilometres wide.
    func testATiltedTurnKeepsTheEyeWhereItStands() throws {
        try loadMap()
        mapView.rotationKeepsEyeAbovePitch = 8
        tilt(70)
        waitUntilFullyRendered()
        let gesture = beginTurn()
        let heading = mapView.camera.heading
        let first = mapView.centerCoordinate
        send(gesture, .changed, .pi / 2)
        let quarter = mapView.centerCoordinate
        send(gesture, .changed, .pi)
        let half = mapView.centerCoordinate
        send(gesture, .ended, .pi)
        // Round the eye the centres keep to one circle; half a turn puts the eye between.
        let eye = CLLocationCoordinate2D(latitude: (first.latitude + half.latitude) / 2,
                                         longitude: (first.longitude + half.longitude) / 2)
        let radius = distance(eye, first)
        XCTAssertGreaterThan(radius, 1000, "the centre lies kilometres ahead")
        XCTAssertEqual(distance(eye, quarter), radius, accuracy: 50, "the quarter turn keeps the circle")
        let behind = (bearing(from: first, to: eye) - heading + 360).truncatingRemainder(dividingBy: 360)
        XCTAssertEqual(behind, 180, accuracy: 5, "the eye is behind the centre it looked at")
    }

    // 138: by default a tilted turn still goes round the centre, as before.
    func testByDefaultATiltedTurnKeepsTheCentre() throws {
        try loadMap()
        XCTAssertEqual(mapView.rotationKeepsEyeAbovePitch, 84)
        tilt(70)
        waitUntilFullyRendered()
        let gesture = beginTurn()
        let center = mapView.centerCoordinate
        turn(gesture, by: .pi / 2)
        XCTAssertLessThan(distance(center, mapView.centerCoordinate), 5)
    }

    /// Beginning, a turn first sets the centre on the terrain it looks at.
    private func beginTurn() -> ScriptedTurn {
        let gesture = ScriptedTurn()
        gesture.at = CGPoint(x: mapView.bounds.midX, y: mapView.bounds.midY)
        send(gesture, .began, 0)
        return gesture
    }

    private func turn(_ gesture: ScriptedTurn, by radians: CGFloat) {
        send(gesture, .changed, radians / 2)
        send(gesture, .changed, radians)
        send(gesture, .ended, radians)
    }

    private func send(_ gesture: ScriptedTurn, _ state: UIGestureRecognizer.State, _ angle: CGFloat) {
        gesture.scripted = state
        gesture.rotation = angle
        mapView.perform(NSSelectorFromString("handleRotateGesture:"), with: gesture)
    }

    private func bearing(from a: CLLocationCoordinate2D, to b: CLLocationCoordinate2D) -> CLLocationDirection {
        let east = (b.longitude - a.longitude) * cos(a.latitude * .pi / 180)
        let north = b.latitude - a.latitude
        return (atan2(east, north) * 180 / .pi + 360).truncatingRemainder(dividingBy: 360)
    }

    private func distance(_ a: CLLocationCoordinate2D, _ b: CLLocationCoordinate2D) -> CLLocationDistance {
        CLLocation(latitude: a.latitude, longitude: a.longitude)
            .distance(from: CLLocation(latitude: b.latitude, longitude: b.longitude))
    }
}
