import ImageIO
import MapLibre
import XCTest

/// 170: a route drawn over terrain stays in sight past the buildings it runs by.
/// A snapshot, since drawHierarchy captures no Metal on the simulator.
final class RouteOverTerrainTests: XCTestCase, MLNMapSnapshotterDelegate {
    private static let center = CLLocationCoordinate2D(latitude: 46.0, longitude: 10.0)
    private var drawnOver = false

    func testTheLayerKeepsTheSwitch() throws {
        let layer = MLNLineStyleLayer(identifier: "route", source: MLNShapeSource(identifier: "s", shape: nil))
        XCTAssertFalse(layer.isDrawnOverTerrain)
        layer.isDrawnOverTerrain = true
        XCTAssertTrue(layer.isDrawnOverTerrain)
    }

    // 170 AI8x on a 3D map the route hid under the buildings and ridges it passed
    func testARouteDrawnOverTerrainShowsOverABuilding() throws {
        drawnOver = false
        let draped = try redArea(in: snapshot())
        drawnOver = true
        let over = try redArea(in: snapshot())
        XCTAssertLessThan(draped, 100, "the building no longer covers a draped route")
        XCTAssertGreaterThan(over, 1000, "the route did not draw over the building")
    }

    func mapSnapshotter(_: MLNMapSnapshotter, didFinishLoading style: MLNStyle) {
        style.layer(withIdentifier: "route")?.isDrawnOverTerrain = drawnOver
    }

    private func snapshot() throws -> MLNMapSnapshot {
        let folder = FileManager.default.temporaryDirectory
            .appendingPathComponent(UUID().uuidString, isDirectory: true)
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: folder) }
        let dem = folder.appendingPathComponent("dem.png")
        try Self.writeFlatHeightModel(to: dem)
        let url = folder.appendingPathComponent("style.json")
        try Self.style(dem: dem).write(to: url)

        let camera = MLNMapCamera(lookingAtCenter: Self.center, acrossDistance: 800, pitch: 0, heading: 0)
        let options = MLNMapSnapshotOptions(styleURL: url, camera: camera, size: CGSize(width: 400, height: 400))
        options.zoomLevel = 16
        let snapshotter = MLNMapSnapshotter(options: options)
        snapshotter.delegate = self
        var result: MLNMapSnapshot?
        let done = expectation(description: "snapshot")
        snapshotter.start { snapshot, _ in
            result = snapshot
            done.fulfill()
        }
        wait(for: [done], timeout: 60)
        return try XCTUnwrap(result)
    }

    /// Red pixels inside the building's square, which the route crosses
    private func redArea(in snapshot: MLNMapSnapshot) throws -> Int {
        let image = try XCTUnwrap(snapshot.image.cgImage)
        let width = image.width, height = image.height
        var rgba = [UInt8](repeating: 0, count: width * height * 4)
        let context = CGContext(data: &rgba, width: width, height: height, bitsPerComponent: 8,
                                bytesPerRow: width * 4, space: CGColorSpaceCreateDeviceRGB(),
                                bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)
        context?.draw(image, in: CGRect(x: 0, y: 0, width: width, height: height))
        let square = CGRect(x: width / 2 - width / 10, y: height / 2 - height / 10,
                            width: width / 5, height: height / 5)
        var red = 0
        for y in Int(square.minY) ..< Int(square.maxY) {
            for x in Int(square.minX) ..< Int(square.maxX) {
                let i = (y * width + x) * 4
                if rgba[i] > 150, rgba[i + 2] < 90 { red += 1 }
            }
        }
        return red
    }

    private static func style(dem: URL) throws -> Data {
        let lon = center.longitude, lat = center.latitude
        let route: [String: Any] = ["type": "LineString", "coordinates": [[lon, lat - 0.01], [lon, lat + 0.01]]]
        let building: [String: Any] = ["type": "Polygon", "coordinates": [[
            [lon - 0.001, lat - 0.0007], [lon + 0.001, lat - 0.0007], [lon + 0.001, lat + 0.0007],
            [lon - 0.001, lat + 0.0007], [lon - 0.001, lat - 0.0007]
        ]]]
        let style: [String: Any] = [
            "version": 8,
            "sources": [
                "dem": ["type": "raster-dem", "tiles": [dem.absoluteString], "tileSize": 256, "maxzoom": 12],
                "route": ["type": "geojson", "data": route],
                "building": ["type": "geojson", "data": building]
            ],
            "terrain": ["source": "dem", "exaggeration": 1],
            "layers": [
                ["id": "background", "type": "background", "paint": ["background-color": "#d8e4c8"]],
                ["id": "building", "type": "fill-extrusion", "source": "building",
                 "paint": ["fill-extrusion-color": "blue", "fill-extrusion-height": 200]],
                ["id": "route", "type": "line", "source": "route",
                 "paint": ["line-color": "red", "line-width": 16]]
            ]
        ]
        return try JSONSerialization.data(withJSONObject: style)
    }

    // mapbox terrain-rgb at sea level: (0 + 10000) * 10
    private static func writeFlatHeightModel(to url: URL) throws {
        let side = 256, value = 100_000
        let pixel: [UInt8] = [UInt8(value >> 16), UInt8((value >> 8) & 255), UInt8(value & 255), 255]
        var bytes = [UInt8]()
        for _ in 0 ..< side * side { bytes += pixel }
        let image = bytes.withUnsafeMutableBytes { buffer -> CGImage? in
            CGContext(data: buffer.baseAddress, width: side, height: side, bitsPerComponent: 8,
                      bytesPerRow: side * 4, space: CGColorSpaceCreateDeviceRGB(),
                      bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)?.makeImage()
        }
        guard let image,
              let png = CGImageDestinationCreateWithURL(url as CFURL, "public.png" as CFString, 1, nil)
        else { throw CocoaError(.fileWriteUnknown) }
        CGImageDestinationAddImage(png, image, nil)
        guard CGImageDestinationFinalize(png) else { throw CocoaError(.fileWriteUnknown) }
    }
}
