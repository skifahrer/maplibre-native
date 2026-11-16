import ImageIO
import MapLibre
import XCTest

/// Every tile has no ground in its west half and stands 2 km high in its east half, so the
/// model's edge runs through each tile and along each tile border. A snapshot, since
/// drawHierarchy captures no Metal on the simulator.
final class ClayFaceTests: XCTestCase {
    private static let plane = 1000.0
    private static let latitude = 49.2
    private static let tileBorder = 2277.0 / 4096 * 360 - 180
    private static let tileMiddle = 2276.5 / 4096 * 360 - 180

    // 143 bug: where the model ended on a tile border its clay face was missing, a slit.
    func testTheClayFaceStandsOnATileBorderAsWithinATile() throws {
        let within = try clayArea(in: snapshot(at: Self.tileMiddle, heading: 90))
        let border = try clayArea(in: snapshot(at: Self.tileBorder, heading: 270))
        XCTAssertGreaterThan(within, 1000, "no clay face within a tile")
        XCTAssertGreaterThan(Double(border), Double(within) / 2, "the border's clay face is missing")
    }

    private func snapshot(at longitude: Double, heading: Double) throws -> MLNMapSnapshot {
        let folder = FileManager.default.temporaryDirectory
            .appendingPathComponent(UUID().uuidString, isDirectory: true)
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: folder) }
        let dem = folder.appendingPathComponent("dem.png")
        try Self.writeHalfGroundModel(to: dem)
        let url = folder.appendingPathComponent("style.json")
        try Self.style(dem: dem).write(to: url)

        let centre = CLLocationCoordinate2D(latitude: Self.latitude, longitude: longitude)
        let camera = MLNMapCamera(lookingAtCenter: centre, acrossDistance: 6000, pitch: 65, heading: heading)
        let options = MLNMapSnapshotOptions(styleURL: url, camera: camera, size: CGSize(width: 400, height: 400))
        options.zoomLevel = 12.3
        let snapshotter = MLNMapSnapshotter(options: options)
        var result: MLNMapSnapshot?
        let done = expectation(description: "snapshot")
        snapshotter.start { snapshot, _ in
            result = snapshot
            done.fulfill()
        }
        wait(for: [done], timeout: 60)
        return try XCTUnwrap(result)
    }

    /// Pixels of the built-in clay: a warm grey, unlike the green table and ground or the sky.
    private func clayArea(in snapshot: MLNMapSnapshot) throws -> Int {
        let image = try XCTUnwrap(snapshot.image.cgImage)
        let width = image.width, height = image.height
        var rgba = [UInt8](repeating: 0, count: width * height * 4)
        let context = CGContext(data: &rgba, width: width, height: height, bitsPerComponent: 8,
                                bytesPerRow: width * 4, space: CGColorSpaceCreateDeviceRGB(),
                                bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)
        context?.draw(image, in: CGRect(x: 0, y: 0, width: width, height: height))
        return stride(from: 0, to: rgba.count, by: 4).filter {
            let r = Int(rgba[$0]), g = Int(rgba[$0 + 1]), b = Int(rgba[$0 + 2])
            return r >= g && g >= b && r - b < 30 && r > 60 && r < 160
        }.count
    }

    private static func style(dem: URL) throws -> Data {
        let style: [String: Any] = [
            "version": 8,
            "sources": [
                "dem": ["type": "raster-dem", "tiles": [dem.absoluteString], "tileSize": 256, "maxzoom": 12]
            ],
            "layers": [["id": "background", "type": "background", "paint": ["background-color": "#d8e4c8"]]],
            "terrain": ["source": "dem", "exaggeration": 1, "plane": plane]
        ]
        return try JSONSerialization.data(withJSONObject: style)
    }

    // mapbox terrain-rgb: height = -10000 + (r * 65536 + g * 256 + b) / 10
    private static func writeHalfGroundModel(to url: URL) throws {
        let side = 256
        var bytes = [UInt8]()
        for _ in 0 ..< side {
            for column in 0 ..< side {
                let value = Int(((column < side / 2 ? plane : 2000) + 10000) * 10)
                bytes += [UInt8(value >> 16), UInt8((value >> 8) & 255), UInt8(value & 255), 255]
            }
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
