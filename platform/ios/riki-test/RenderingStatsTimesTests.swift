import MapLibre
import XCTest

/// 142 the stats tell the wait for a drawable and the GPU's time apart from the CPU's work.
final class RenderingStatsTimesTests: TerrainMapTestCase {
    private var frames: [MLNRenderingStats] = []

    func mapViewDidFinishRenderingFrame(_: MLNMapView, fullyRendered _: Bool,
                                        renderingStats: MLNRenderingStats) {
        frames.append(renderingStats)
    }

    // 142 after a few frames the GPU has a time, and neither is a second or more
    func testFramesReportTheirWaitAndGpuTime() throws {
        try loadMap(terrain: false)
        for step in 0 ..< 12 {
            mapView.setCenter(CLLocationCoordinate2D(latitude: Self.center.latitude,
                                                     longitude: Self.center.longitude + Double(step) * 0.002),
                              zoomLevel: 12, animated: false)
            waitUntilFullyRendered()
        }
        XCTAssertGreaterThan(frames.count, 3, "too few frames to read a GPU time from")
        XCTAssertTrue(frames.contains { $0.gpuTime > 0 }, "no frame reported the GPU's time")
        for stats in frames {
            XCTAssertGreaterThanOrEqual(stats.drawableWaitTime, 0)
            XCTAssertLessThan(stats.drawableWaitTime, 1)
            XCTAssertLessThan(stats.gpuTime, 1)
        }
    }

    // 151 frames drawn back to back each bring a GPU time, not one held over many frames
    func testGpuTimeFollowsFramesDrawnBackToBack() throws {
        try loadMap()
        tilt(70)
        frames.removeAll()
        for step in 0 ..< 90 {
            mapView.setCenter(CLLocationCoordinate2D(latitude: Self.center.latitude,
                                                     longitude: Self.center.longitude + Double(step) * 0.0005),
                              zoomLevel: 13, animated: false)
            RunLoop.main.run(until: Date(timeIntervalSinceNow: 1.0 / 60))
        }
        let times = frames.map(\.gpuTime).filter { $0 > 0 }
        XCTAssertGreaterThan(times.count, 20, "too few frames with a GPU time")
        let changes = zip(times, times.dropFirst()).filter { $0.0 != $0.1 }.count
        XCTAssertGreaterThan(Double(changes), Double(times.count) / 2,
                             "the GPU time stayed one value over \(times.count - changes) frames")
    }
}
