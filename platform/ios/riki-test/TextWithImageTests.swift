import MapLibre
import XCTest

/// 135: Metal's text-and-icon program with a colour per feature never compiled.
/// A snapshot, since drawHierarchy captures no Metal on the simulator.
final class TextWithImageTests: XCTestCase, MLNMapSnapshotterDelegate {
    private static let center = CLLocationCoordinate2D(latitude: 48.15, longitude: 17.10)

    // 135 an image in a text field, coloured per feature, drew nothing
    func testAnImageInATextFieldColouredPerFeatureDraws() throws {
        XCTAssertGreaterThan(try blueArea(in: snapshot()), 100)
    }

    func mapSnapshotter(_: MLNMapSnapshotter, didFinishLoading style: MLNStyle) {
        let image = UIGraphicsImageRenderer(size: CGSize(width: 28, height: 28)).image { context in
            UIColor.blue.setFill()
            context.fill(CGRect(x: 0, y: 0, width: 28, height: 28))
        }
        style.setImage(image, forName: "icon")
    }

    private func snapshot() throws -> MLNMapSnapshot {
        let folder = FileManager.default.temporaryDirectory
            .appendingPathComponent(UUID().uuidString, isDirectory: true)
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: folder) }
        let url = folder.appendingPathComponent("style.json")
        try Self.style().write(to: url)

        let camera = MLNMapCamera(lookingAtCenter: Self.center, acrossDistance: 1000, pitch: 0, heading: 0)
        let options = MLNMapSnapshotOptions(styleURL: url, camera: camera, size: CGSize(width: 200, height: 200))
        options.zoomLevel = 16
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

    private func blueArea(in snapshot: MLNMapSnapshot) throws -> Int {
        let image = try XCTUnwrap(snapshot.image.cgImage)
        let width = image.width, height = image.height
        var rgba = [UInt8](repeating: 0, count: width * height * 4)
        let context = CGContext(data: &rgba, width: width, height: height, bitsPerComponent: 8,
                                bytesPerRow: width * 4, space: CGColorSpaceCreateDeviceRGB(),
                                bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)
        context?.draw(image, in: CGRect(x: 0, y: 0, width: width, height: height))
        return stride(from: 0, to: rgba.count, by: 4).filter { rgba[$0 + 2] > 200 && rgba[$0] < 60 }.count
    }

    private static func style() throws -> Data {
        let point: [String: Any] = [
            "type": "Feature", "properties": ["colour": "#ff0000"],
            "geometry": ["type": "Point", "coordinates": [center.longitude, center.latitude]]
        ]
        let style: [String: Any] = [
            "version": 8,
            "sources": ["point": ["type": "geojson", "data": point]],
            "layers": [
                ["id": "background", "type": "background", "paint": ["background-color": "#ffffff"]],
                ["id": "label", "type": "symbol", "source": "point",
                 "layout": ["text-field": ["format", ["image", "icon"], [String: Any]()],
                            "text-allow-overlap": true, "text-ignore-placement": true],
                 "paint": ["text-color": ["get", "colour"]]]
            ]
        ]
        return try JSONSerialization.data(withJSONObject: style)
    }
}
