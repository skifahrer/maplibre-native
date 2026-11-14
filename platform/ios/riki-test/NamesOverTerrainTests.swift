import ImageIO
import MapLibre
import XCTest

/// 135: a name hanging below its anchor over a nearer ridge draws whole, as on a saddle.
/// A snapshot, since drawHierarchy captures no Metal on the simulator.
final class NamesOverTerrainTests: XCTestCase, MLNMapSnapshotterDelegate {
    private static let world = 4096.0 * 256
    private static let tileRow = 1403.0
    private static let anchorRow = 40.0
    private static let anchor = CLLocationCoordinate2D(latitude: latitude(atRow: anchorRow), longitude: 20.08)
    private static let metresPerRow = 40_075_016.686 * cos(anchor.latitude * .pi / 180) / world
    /// South of the anchor and high enough to hide the ground under the hanging icon.
    private static let ridgeRow = anchorRow + 800 / metresPerRow
    private static let ridgeHeight = 200.0
    private static let icon = 28

    // 135 the slopes beside a saddle cut its name's ends off on a tilted 3D map
    func testANameHangingOverANearerRidgeDrawsWhole() throws {
        let blue = try blueArea(in: snapshot(terrain: true))
        let whole = try blueArea(in: snapshot(terrain: false))
        XCTAssertGreaterThan(whole, 0)
        XCTAssertGreaterThanOrEqual(Double(blue), Double(whole) * 0.9, "the ridge cut the icon")
    }

    func mapSnapshotter(_: MLNMapSnapshotter, didFinishLoading style: MLNStyle) {
        let side = CGFloat(Self.icon)
        let image = UIGraphicsImageRenderer(size: CGSize(width: side, height: side)).image { context in
            UIColor.blue.setFill()
            context.fill(CGRect(x: 0, y: 0, width: side, height: side))
        }
        style.setImage(image, forName: "icon")
    }

    private static func latitude(atRow row: Double) -> Double {
        let y = (tileRow * 256 + row) / world
        return atan(sinh(.pi * (1 - 2 * y))) * 180 / .pi
    }

    private func snapshot(terrain: Bool) throws -> MLNMapSnapshot {
        let folder = FileManager.default.temporaryDirectory
            .appendingPathComponent(UUID().uuidString, isDirectory: true)
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: folder) }
        let dem = folder.appendingPathComponent("dem.png")
        try Self.writeRidgedHeightModel(to: dem)
        let url = folder.appendingPathComponent("style.json")
        try Self.style(dem: dem, terrain: terrain).write(to: url)

        let camera = MLNMapCamera(lookingAtCenter: Self.anchor, acrossDistance: 3000, pitch: 60, heading: 0)
        let options = MLNMapSnapshotOptions(styleURL: url, camera: camera, size: CGSize(width: 400, height: 400))
        options.zoomLevel = 13.3
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

    private static func style(dem: URL, terrain: Bool) throws -> Data {
        let icons: [String: Any] = ["type": "Point", "coordinates": [anchor.longitude, anchor.latitude]]
        var style: [String: Any] = [
            "version": 8,
            "sources": [
                "dem": ["type": "raster-dem", "tiles": [dem.absoluteString], "tileSize": 256, "maxzoom": 12],
                "icons": ["type": "geojson", "data": icons]
            ],
            "layers": [
                ["id": "background", "type": "background", "paint": ["background-color": "#d8e4c8"]],
                ["id": "hillshade", "type": "hillshade", "source": "dem"],
                ["id": "icons", "type": "symbol", "source": "icons",
                 "layout": ["icon-image": "icon", "icon-anchor": "top", "icon-offset": [0, 50],
                            "icon-allow-overlap": true, "icon-ignore-placement": true]]
            ]
        ]
        if terrain { style["terrain"] = ["source": "dem", "exaggeration": 1] }
        return try JSONSerialization.data(withJSONObject: style)
    }

    // mapbox terrain-rgb: height = -10000 + (r * 65536 + g * 256 + b) / 10
    private static func writeRidgedHeightModel(to url: URL) throws {
        let side = 256
        var bytes = [UInt8]()
        for row in 0 ..< side {
            let rise = max(0, ridgeHeight - abs(Double(row) - ridgeRow) * metresPerRow)
            let value = Int((1000 + rise + 10000) * 10)
            let pixel: [UInt8] = [UInt8(value >> 16), UInt8((value >> 8) & 255), UInt8(value & 255), 255]
            for _ in 0 ..< side { bytes += pixel }
        }
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
