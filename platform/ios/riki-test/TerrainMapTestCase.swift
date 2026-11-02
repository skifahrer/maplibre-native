import ImageIO
import MapLibre
import XCTest

/// A map view in a window over a flat height model at `groundHeight` metres.
class TerrainMapTestCase: XCTestCase, MLNMapViewDelegate {
    static let groundHeight = 1000.0
    static let center = CLLocationCoordinate2D(latitude: 49.18, longitude: 20.08)

    var mapView: MLNMapView!
    var window: UIWindow!
    private var styleLoaded: XCTestExpectation?
    private var fullyRendered: XCTestExpectation?
    private var folder: URL!

    func loadMap(terrain: Bool = true) throws {
        folder = FileManager.default.temporaryDirectory
            .appendingPathComponent(UUID().uuidString, isDirectory: true)
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        let dem = folder.appendingPathComponent("dem.png")
        try Self.writeFlatHeightModel(to: dem)
        let style = folder.appendingPathComponent("style.json")
        try Self.style(dem: dem, terrain: terrain).write(to: style)

        window = UIWindow(frame: CGRect(x: 0, y: 0, width: 390, height: 844))
        mapView = MLNMapView(frame: window.bounds, styleURL: style)
        mapView.delegate = self
        window.addSubview(mapView)
        window.makeKeyAndVisible()
        mapView.setCenter(Self.center, zoomLevel: 12, animated: false)

        styleLoaded = expectation(description: "style loads")
        wait(for: [styleLoaded!], timeout: 30)
    }

    func waitUntilFullyRendered() {
        fullyRendered = expectation(description: "map renders fully")
        mapView.perform(NSSelectorFromString("setNeedsRerender"))
        wait(for: [fullyRendered!], timeout: 60)
        fullyRendered = nil
    }

    func tilt(_ pitch: CGFloat, distance: CLLocationDistance = 8000) {
        let camera = MLNMapCamera(lookingAtCenter: Self.center, acrossDistance: distance,
                                  pitch: pitch, heading: 30)
        mapView.setCamera(camera, animated: false)
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

    // idle comes after every frame that left nothing to load
    func mapViewDidBecomeIdle(_: MLNMapView) {
        fullyRendered?.fulfill()
        fullyRendered = nil
    }

    // a url without {z}/{x}/{y} serves the same tile everywhere: flat ground
    static func style(dem: URL, terrain: Bool) throws -> Data {
        var style: [String: Any] = [
            "version": 8,
            "sources": [
                "dem": ["type": "raster-dem", "tiles": [dem.absoluteString],
                        "tileSize": 256, "maxzoom": 12]
            ],
            "layers": [
                ["id": "background", "type": "background",
                 "paint": ["background-color": "#d8e4c8"]],
                ["id": "hillshade", "type": "hillshade", "source": "dem"]
            ]
        ]
        if terrain { style["terrain"] = ["source": "dem", "exaggeration": 1] }
        return try JSONSerialization.data(withJSONObject: style)
    }

    // mapbox terrain-rgb: height = -10000 + (r * 65536 + g * 256 + b) / 10
    static func writeFlatHeightModel(to url: URL) throws {
        let value = Int((groundHeight + 10000) * 10)
        let pixel: [UInt8] = [UInt8(value >> 16), UInt8((value >> 8) & 255), UInt8(value & 255), 255]
        let side = 256
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
