import MapLibre
import XCTest

/// A snapshot, since drawHierarchy captures no Metal on the simulator.
final class SymbolDepthTests: XCTestCase, MLNMapSnapshotterDelegate {
    private static let center = CLLocationCoordinate2D(latitude: 48.15, longitude: 17.10)
    private static let behind = CLLocationCoordinate2D(latitude: 48.15 + 40 / 111_000, longitude: 17.10)
    private static let front = CLLocationCoordinate2D(latitude: 48.15 - 40 / 111_000, longitude: 17.10)

    func testABuildingHidesTheIconBehindItOnATiltedMap() throws {
        let snapshot = try snapshot(pitch: 60)
        XCTAssertFalse(isIcon(snapshot, at: Self.behind))
        XCTAssertTrue(isIcon(snapshot, at: Self.front))
    }

    func testFromAboveAnIconStaysOverTheRoofs() throws {
        XCTAssertTrue(isIcon(try snapshot(pitch: 20), at: Self.center))
    }

    func mapSnapshotter(_: MLNMapSnapshotter, didFinishLoading style: MLNStyle) {
        let image = UIGraphicsImageRenderer(size: CGSize(width: 28, height: 28)).image { context in
            UIColor.blue.setFill()
            context.fill(CGRect(x: 0, y: 0, width: 28, height: 28))
        }
        style.setImage(image, forName: "icon")
    }

    private func snapshot(pitch: CGFloat) throws -> MLNMapSnapshot {
        let folder = FileManager.default.temporaryDirectory
            .appendingPathComponent(UUID().uuidString, isDirectory: true)
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: folder) }
        let url = folder.appendingPathComponent("style.json")
        try Self.style().write(to: url)

        let camera = MLNMapCamera(lookingAtCenter: Self.center, acrossDistance: 400, pitch: pitch, heading: 0)
        let options = MLNMapSnapshotOptions(styleURL: url, camera: camera,
                                            size: CGSize(width: 400, height: 400))
        options.zoomLevel = 17.5
        let snapshotter = MLNMapSnapshotter(options: options)
        snapshotter.delegate = self
        var result: MLNMapSnapshot?
        let done = expectation(description: "snapshot")
        snapshotter.start { snapshot, _ in
            result = snapshot
            done.fulfill()
        }
        wait(for: [done], timeout: 30)
        return try XCTUnwrap(result)
    }

    private func isIcon(_ snapshot: MLNMapSnapshot, at coordinate: CLLocationCoordinate2D) -> Bool {
        let image = snapshot.image
        let point = snapshot.point(for: coordinate)
        let pixel = CGRect(x: point.x * image.scale, y: point.y * image.scale, width: 1, height: 1)
        guard let cropped = image.cgImage?.cropping(to: pixel) else { return false }
        var rgba = [UInt8](repeating: 0, count: 4)
        let context = CGContext(data: &rgba, width: 1, height: 1, bitsPerComponent: 8, bytesPerRow: 4,
                                space: CGColorSpaceCreateDeviceRGB(),
                                bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)
        context?.draw(cropped, in: CGRect(x: 0, y: 0, width: 1, height: 1))
        return rgba[2] > 200 && rgba[0] < 60
    }

    private static func style() throws -> Data {
        let east = 40 / 74_000.0, north = 12 / 111_000.0
        let lon = center.longitude, lat = center.latitude
        let building: [String: Any] = [
            "type": "Polygon",
            "coordinates": [[[lon - east, lat - north], [lon + east, lat - north], [lon + east, lat + north],
                             [lon - east, lat + north], [lon - east, lat - north]]]
        ]
        let icons: [String: Any] = [
            "type": "MultiPoint",
            "coordinates": [[behind.longitude, behind.latitude], [front.longitude, front.latitude], [lon, lat]]
        ]
        let style: [String: Any] = [
            "version": 8,
            "sources": [
                "building": ["type": "geojson", "data": building],
                "icons": ["type": "geojson", "data": icons]
            ],
            "layers": [
                ["id": "background", "type": "background", "paint": ["background-color": "#cce8cc"]],
                ["id": "building", "type": "fill-extrusion", "source": "building",
                 "paint": ["fill-extrusion-color": "#d05050", "fill-extrusion-height": 40]],
                ["id": "icons", "type": "symbol", "source": "icons",
                 "layout": ["icon-image": "icon", "icon-allow-overlap": true, "icon-ignore-placement": true]]
            ]
        ]
        return try JSONSerialization.data(withJSONObject: style)
    }
}
