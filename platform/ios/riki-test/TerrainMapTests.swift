import MapLibre
import XCTest

/// What our patches promise the app, drawn on a simulator.
final class TerrainMapTests: TerrainMapTestCase {
    func testTerrainRendersFullyInEveryLoadAndLodMode() throws {
        try loadMap()
        let loads: [MLNTerrainLoadMode] = [.quality, .balanced, .performance]
        let lods: [MLNTileLodMode] = [.default, .distance, .adaptive]
        for (index, load) in loads.enumerated() {
            for lod in lods {
                mapView.terrainLoadMode = load
                mapView.tileLodMode = lod
                tilt(CGFloat(45 + index * 10))
                waitUntilFullyRendered()
                XCTAssertEqual(mapView.terrainLoadMode, load)
                XCTAssertEqual(mapView.tileLodMode, lod)
            }
        }
    }

    func testSightMeetsTheGroundOnlyWithTerrain() throws {
        try loadMap()
        tilt(0)
        waitUntilFullyRendered()
        waitUntilFullyRendered()
        let ground = mapView.sightGroundDistance
        XCTAssertTrue(ground.isFinite, "no ground under the centre: \(ground)")
        XCTAssertGreaterThan(ground, 0)
        XCTAssertLessThan(ground, mapView.camera.altitude)
    }

    func testSightMeetsNoGroundWithoutTerrain() throws {
        try loadMap(terrain: false)
        tilt(0)
        waitUntilFullyRendered()
        XCTAssertTrue(mapView.sightGroundDistance.isNaN)
    }

    func testAnchoringTheCentreOnTerrainMovesNothingOnScreen() throws {
        try loadMap()
        tilt(55)
        waitUntilFullyRendered()
        waitUntilFullyRendered()
        let corner = CGPoint(x: mapView.bounds.width * 0.2, y: mapView.bounds.height * 0.8)
        let before = mapView.convert(corner, toCoordinateFrom: mapView)
        let zoom = mapView.zoomLevel

        mapView.anchorCenterOnTerrain()
        waitUntilFullyRendered()

        let after = mapView.convert(corner, toCoordinateFrom: mapView)
        XCTAssertEqual(after.latitude, before.latitude, accuracy: 1e-4)
        XCTAssertEqual(after.longitude, before.longitude, accuracy: 1e-4)
        XCTAssertGreaterThanOrEqual(mapView.zoomLevel, zoom - 1e-6)
    }

    func testACentreInTheAirLooksAboveTheHorizon() throws {
        try loadMap()
        mapView.maximumPitch = 150
        let altitude = Self.groundHeight + 120
        mapView.setCenter(Self.center, altitude: altitude, zoomLevel: 17, direction: 30, pitch: 120)
        waitUntilFullyRendered()

        XCTAssertEqual(mapView.centerAltitude, altitude, accuracy: 0.5)
        XCTAssertEqual(mapView.camera.pitch, 120, accuracy: 0.01)
        XCTAssertEqual(mapView.direction, 30, accuracy: 0.01)
        XCTAssertEqual(mapView.zoomLevel, 17, accuracy: 1e-6)
        XCTAssertEqual(mapView.centerCoordinate.latitude, Self.center.latitude, accuracy: 1e-6)
    func testTheSkyColourPaintsOnlyAboveTheHorizon() throws {
        try loadMap()
        XCTAssertNil(mapView.skyColor)
        mapView.skyColor = UIColor(red: 0.3, green: 0.55, blue: 0.9, alpha: 1)
        mapView.drawDistance = 3000
        tilt(85, distance: 1500)
        waitUntilFullyRendered()

        assertColor(mapView.skyColor, 0.3, 0.55, 0.9)
        let shot = try XCTUnwrap(screenshot())
        assertPixel(shot, at: CGPoint(x: 0.5, y: 0.02), 0.3, 0.55, 0.9)
        assertPixel(shot, at: CGPoint(x: 0.5, y: 0.98), 0.3, 0.55, 0.9, matches: false)

        mapView.skyColor = nil
        XCTAssertNil(mapView.skyColor)
    }

    func testDrawDistanceFogAndClayKeepTheirValuesAndDraw() throws {
        try loadMap()
        XCTAssertEqual(mapView.drawDistance, 0)
        XCTAssertNil(mapView.fogColor)
        XCTAssertNil(mapView.clayColor)

        mapView.drawDistance = 20000
        mapView.fogColor = UIColor(red: 0.8, green: 0.85, blue: 0.9, alpha: 1)
        mapView.fogStartDistance = 5000
        mapView.fogEndDistance = 20000
        mapView.clayColor = UIColor(red: 0.3, green: 0.25, blue: 0.2, alpha: 1)
        tilt(75, distance: 3000)
        waitUntilFullyRendered()

        XCTAssertEqual(mapView.drawDistance, 20000, accuracy: 0.5)
        XCTAssertEqual(mapView.fogStartDistance, 5000, accuracy: 0.5)
        XCTAssertEqual(mapView.fogEndDistance, 20000, accuracy: 0.5)
        assertColor(mapView.fogColor, 0.8, 0.85, 0.9)
        assertColor(mapView.clayColor, 0.3, 0.25, 0.2)

        mapView.fogColor = nil
        mapView.drawDistance = 0
        waitUntilFullyRendered()
        XCTAssertNil(mapView.fogColor)
        XCTAssertEqual(mapView.drawDistance, 0)
    }

    private func assertColor(_ color: UIColor?, _ red: CGFloat, _ green: CGFloat, _ blue: CGFloat,
                             file: StaticString = #filePath, line: UInt = #line) {
        var r: CGFloat = 0, g: CGFloat = 0, b: CGFloat = 0, a: CGFloat = 0
        guard let color, color.getRed(&r, green: &g, blue: &b, alpha: &a) else {
            return XCTFail("no colour", file: file, line: line)
        }
        XCTAssertEqual(r, red, accuracy: 0.01, file: file, line: line)
        XCTAssertEqual(g, green, accuracy: 0.01, file: file, line: line)
        XCTAssertEqual(b, blue, accuracy: 0.01, file: file, line: line)
    }

    private func screenshot() -> CGImage? {
        let format = UIGraphicsImageRendererFormat()
        format.scale = 1
        return UIGraphicsImageRenderer(bounds: mapView.bounds, format: format).image { _ in
            mapView.drawHierarchy(in: mapView.bounds, afterScreenUpdates: true)
        }.cgImage
    }

    // `at` is a share of the width and height, from the top left
    private func assertPixel(_ image: CGImage, at point: CGPoint, _ red: CGFloat, _ green: CGFloat,
                             _ blue: CGFloat, matches: Bool = true,
                             file: StaticString = #filePath, line: UInt = #line) {
        var pixel = [UInt8](repeating: 0, count: 4)
        let x = Int(point.x * CGFloat(image.width - 1)), y = Int(point.y * CGFloat(image.height - 1))
        pixel.withUnsafeMutableBytes { buffer in
            let context = CGContext(data: buffer.baseAddress, width: 1, height: 1, bitsPerComponent: 8,
                                    bytesPerRow: 4, space: CGColorSpaceCreateDeviceRGB(),
                                    bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)
            context?.draw(image, in: CGRect(x: -x, y: y - image.height + 1, width: image.width,
                                            height: image.height))
        }
        let near = [red, green, blue].enumerated().allSatisfy { abs(CGFloat(pixel[$0]) / 255 - $1) < 0.04 }
        XCTAssertEqual(near, matches, "pixel \(pixel) at \(point)", file: file, line: line)
    }
}
