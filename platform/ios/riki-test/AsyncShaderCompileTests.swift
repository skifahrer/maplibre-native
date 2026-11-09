import MapLibre
import XCTest

/// A live map whose layers each need a shader variant of their own.
final class AsyncShaderCompileTests: XCTestCase, MLNMapViewDelegate {
    private static let center = CLLocationCoordinate2D(latitude: 48.15, longitude: 17.10)
    private var window: UIWindow!
    private var mapView: MLNMapView!
    private var styleLoaded: XCTestExpectation?
    private var idle: XCTestExpectation?
    private var folder: URL!

    // 97 off unless the app turns it on, so snapshots and old callers still wait
    func testTheSwitchIsOffUntilTurnedOn() throws {
        try loadMap(async: false)
        XCTAssertFalse(mapView.compilesShadersAsynchronously)
        mapView.compilesShadersAsynchronously = true
        XCTAssertTrue(mapView.compilesShadersAsynchronously)
    }

    // 97 switched on, each skipped layer gets the frame that draws it, so the map settles
    func testAMapThatSkipsLayersWhileCompilingStillSettles() throws {
        try loadMap(async: true)
        idle = expectation(description: "map settles once every compile has landed")
        mapView.perform(NSSelectorFromString("setNeedsRerender"))
        wait(for: [idle!], timeout: 60)
    }

    private func loadMap(async: Bool) throws {
        folder = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        let url = folder.appendingPathComponent("style.json")
        try Self.style().write(to: url)
        window = UIWindow(frame: CGRect(x: 0, y: 0, width: 390, height: 844))
        mapView = MLNMapView(frame: window.bounds, styleURL: url)
        mapView.compilesShadersAsynchronously = async
        mapView.delegate = self
        window.addSubview(mapView)
        window.makeKeyAndVisible()
        mapView.setCenter(Self.center, zoomLevel: 15, animated: false)
        styleLoaded = expectation(description: "style loads")
        wait(for: [styleLoaded!], timeout: 30)
    }

    override func tearDown() {
        mapView?.removeFromSuperview()
        mapView = nil
        window = nil
        if let folder { try? FileManager.default.removeItem(at: folder) }
        super.tearDown()
    }

    func mapView(_: MLNMapView, didFinishLoading _: MLNStyle) {
        styleLoaded?.fulfill()
        styleLoaded = nil
    }

    func mapViewDidBecomeIdle(_: MLNMapView) {
        idle?.fulfill()
        idle = nil
    }

    // each layer reads other data-driven properties, so each compiles its own variant
    private static func style() throws -> Data {
        let points: [[String: Any]] = (0..<12).map { index in
            ["type": "Feature",
             "properties": ["color": "#3366ff", "size": 4 + index, "fade": 0.8, "edge": 2],
             "geometry": ["type": "Point",
                          "coordinates": [center.longitude + Double(index) * 0.0004, center.latitude]]]
        }
        let read: [String: [Any]] = ["color": ["get", "color"], "size": ["get", "size"],
                                     "fade": ["get", "fade"], "edge": ["get", "edge"]]
        let properties = ["circle-color": "color", "circle-radius": "size", "circle-opacity": "fade",
                          "circle-stroke-width": "edge", "circle-blur": "fade", "circle-stroke-opacity": "fade"]
        let names = Array(properties.keys).sorted()
        let layers: [[String: Any]] = (0..<12).map { index in
            var paint: [String: Any] = ["circle-radius": 6, "circle-color": "#ff0000"]
            for (bit, name) in names.enumerated() where index & (1 << (bit % 4)) != 0 || bit == index % 6 {
                paint[name] = read[properties[name]!]!
            }
            return ["id": "circles-\(index)", "type": "circle", "source": "points", "paint": paint]
        }
        let style: [String: Any] = [
            "version": 8,
            "sources": ["points": ["type": "geojson",
                                   "data": ["type": "FeatureCollection", "features": points]]],
            "layers": [["id": "background", "type": "background", "paint": ["background-color": "#ffffff"]]]
                + layers
        ]
        return try JSONSerialization.data(withJSONObject: style)
    }
}
