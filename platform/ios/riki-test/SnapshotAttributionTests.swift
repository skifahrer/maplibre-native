import MapLibre
import ObjectiveC
import XCTest

/// A snapshot asked to draw no attribution parses none.
final class SnapshotAttributionTests: XCTestCase {
    private var folder: URL!
    private static var parses = 0
    private static var original: IMP?

    // 129 Riki Maps build 1125 hung 0.55 s parsing attribution HTML for a snapshot that drew none
    func testASnapshotWithoutAttributionParsesNone() throws {
        Self.parses = 0
        _ = try snapshot(showingAttribution: false)
        XCTAssertEqual(Self.parses, 0)
    }

    // 129 one that draws it still parses each source's attribution
    func testASnapshotWithAttributionParsesIt() throws {
        Self.parses = 0
        _ = try snapshot(showingAttribution: true)
        XCTAssertGreaterThan(Self.parses, 0)
    }

    private typealias Parse = @convention(c) (AnyClass, Selector, NSString?, CGFloat, UIColor?) -> NSArray
    private static let selector = NSSelectorFromString("attributionInfosFromHTMLString:fontSize:linkColor:")

    override func setUpWithError() throws {
        try super.setUpWithError()
        folder = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        let info: AnyClass = try XCTUnwrap(NSClassFromString("MLNAttributionInfo"))
        let method = try XCTUnwrap(class_getClassMethod(info, Self.selector))
        let original = method_getImplementation(method)
        Self.original = original
        let parse = unsafeBitCast(original, to: Parse.self)
        let counting: @convention(block) (AnyClass, NSString?, CGFloat, UIColor?) -> NSArray = { cls, html, size, color in
            Self.parses += 1
            return parse(cls, Self.selector, html, size, color)
        }
        method_setImplementation(method, imp_implementationWithBlock(counting))
    }

    override func tearDown() {
        if let original = Self.original, let info = NSClassFromString("MLNAttributionInfo"),
           let method = class_getClassMethod(info, Self.selector) {
            method_setImplementation(method, original)
        }
        if let folder { try? FileManager.default.removeItem(at: folder) }
        super.tearDown()
    }

    private func snapshot(showingAttribution: Bool) throws -> MLNMapSnapshot {
        let url = folder.appendingPathComponent("style.json")
        // The source starts deeper than the snapshot looks, so it loads no tiles.
        try Data("""
        {"version": 8,
         "sources": {"photos": {"type": "raster", "tiles": ["file:///nowhere/{z}/{x}/{y}.png"],
                                "minzoom": 20, "attribution": "<a href=\\"https://example.com\\">Example</a>"}},
         "layers": [{"id": "ground", "type": "background", "paint": {"background-color": "white"}},
                    {"id": "photos", "type": "raster", "source": "photos"}]}
        """.utf8).write(to: url)
        let camera = MLNMapCamera(lookingAtCenter: CLLocationCoordinate2D(latitude: 48.15, longitude: 17.1),
                                  altitude: 1, pitch: 0, heading: 0)
        let options = MLNMapSnapshotOptions(styleURL: url, camera: camera, size: CGSize(width: 320, height: 240))
        options.zoomLevel = 2
        options.showsAttribution = showingAttribution
        options.showsLogo = false
        let snapshotter = MLNMapSnapshotter(options: options)
        let done = expectation(description: "snapshot")
        var result: Result<MLNMapSnapshot, Error>?
        snapshotter.start { snapshot, error in
            result = snapshot.map { .success($0) } ?? .failure(error ?? CocoaError(.featureUnsupported))
            done.fulfill()
        }
        wait(for: [done], timeout: 60)
        return try XCTUnwrap(result).get()
    }
}
