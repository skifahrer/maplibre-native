import MapLibre
import XCTest

/// A snapshot, since drawHierarchy captures no Metal on the simulator.
final class ShaderCompileTests: XCTestCase {
    private static let center = CLLocationCoordinate2D(latitude: 48.15, longitude: 17.10)
    private static let west = CLLocationCoordinate2D(latitude: 48.15, longitude: 17.10 - 0.002)
    private static let east = CLLocationCoordinate2D(latitude: 48.15, longitude: 17.10 + 0.002)
    private static let north = CLLocationCoordinate2D(latitude: 48.15 + 0.0012, longitude: 17.10)

    // 96 programs compiled side by side in one frame each draw with their own data-driven colour
    func testEveryLayerTheFirstFrameCompilesDrawsInItsColour() throws {
        let snapshot = try snapshot()
        XCTAssertEqual(color(snapshot, at: Self.west), "red")
        XCTAssertEqual(color(snapshot, at: Self.east), "blue")
        XCTAssertEqual(color(snapshot, at: Self.north), "green")
        XCTAssertEqual(color(snapshot, at: Self.center), "background")
    }

    private func snapshot() throws -> MLNMapSnapshot {
        let folder = FileManager.default.temporaryDirectory
            .appendingPathComponent(UUID().uuidString, isDirectory: true)
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: folder) }
        let url = folder.appendingPathComponent("style.json")
        try Self.style().write(to: url)

        let camera = MLNMapCamera(lookingAtCenter: Self.center, acrossDistance: 1000, pitch: 0, heading: 0)
        let options = MLNMapSnapshotOptions(styleURL: url, camera: camera,
                                            size: CGSize(width: 400, height: 400))
        options.zoomLevel = 16
        let snapshotter = MLNMapSnapshotter(options: options)
        var result: MLNMapSnapshot?
        let done = expectation(description: "snapshot")
        snapshotter.start { snapshot, _ in
            result = snapshot
            done.fulfill()
        }
        wait(for: [done], timeout: 30)
        return try XCTUnwrap(result)
    }

    private func color(_ snapshot: MLNMapSnapshot, at coordinate: CLLocationCoordinate2D) -> String {
        let image = snapshot.image
        let point = snapshot.point(for: coordinate)
        let pixel = CGRect(x: point.x * image.scale, y: point.y * image.scale, width: 1, height: 1)
        guard let cropped = image.cgImage?.cropping(to: pixel) else { return "none" }
        var rgba = [UInt8](repeating: 0, count: 4)
        let context = CGContext(data: &rgba, width: 1, height: 1, bitsPerComponent: 8, bytesPerRow: 4,
                                space: CGColorSpaceCreateDeviceRGB(),
                                bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)
        context?.draw(cropped, in: CGRect(x: 0, y: 0, width: 1, height: 1))
        switch (rgba[0] > 200, rgba[1] > 200, rgba[2] > 200) {
        case (true, false, false): return "red"
        case (false, true, false): return "green"
        case (false, false, true): return "blue"
        case (true, true, true): return "background"
        default: return "other \(rgba)"
        }
    }

    private static func feature(_ geometry: [String: Any], color: String) -> [String: Any] {
        ["type": "Feature", "properties": ["color": color], "geometry": geometry]
    }

    private static func style() throws -> Data {
        let (lat, lon) = (center.latitude, center.longitude)
        let square: [[Double]] = [[lon - 0.003, lat - 0.0005], [lon - 0.001, lat - 0.0005],
                                  [lon - 0.001, lat + 0.0005], [lon - 0.003, lat + 0.0005],
                                  [lon - 0.003, lat - 0.0005]]
        let features: [[String: Any]] = [
            feature(["type": "Polygon", "coordinates": [square]], color: "#ff0000"),
            feature(["type": "LineString", "coordinates": [[lon - 0.002, north.latitude],
                                                           [lon + 0.002, north.latitude]]], color: "#00ff00"),
            feature(["type": "Point", "coordinates": [east.longitude, east.latitude]], color: "#0000ff")
        ]
        let byColor: [Any] = ["get", "color"]
        let style: [String: Any] = [
            "version": 8,
            "sources": ["shapes": ["type": "geojson",
                                   "data": ["type": "FeatureCollection", "features": features]]],
            "layers": [
                ["id": "background", "type": "background", "paint": ["background-color": "#ffffff"]],
                ["id": "fill", "type": "fill", "source": "shapes",
                 "filter": ["==", ["geometry-type"], "Polygon"], "paint": ["fill-color": byColor]],
                ["id": "line", "type": "line", "source": "shapes",
                 "filter": ["==", ["geometry-type"], "LineString"],
                 "paint": ["line-color": byColor, "line-width": 12]],
                ["id": "circle", "type": "circle", "source": "shapes",
                 "filter": ["==", ["geometry-type"], "Point"],
                 "paint": ["circle-color": byColor, "circle-radius": 14]]
            ]
        ]
        return try JSONSerialization.data(withJSONObject: style)
    }
}
