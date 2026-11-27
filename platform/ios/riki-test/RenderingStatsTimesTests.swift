import MapLibre
import XCTest

/// 142 the stats tell the wait for a drawable and the GPU's time apart from the CPU's work.
final class RenderingStatsTimesTests: TerrainMapTestCase {
    /// 151 the map view hands one stats object over and over, so each frame's times are copied
    private var frames: [(wait: Double, gpu: Double)] = []
    /// 197 the last frame's terrain counters, which are totals
    private var counters: (made: Int32, freed: Int32, drapes: Int32, vertices: Int64) = (0, 0, 0, 0)

    func mapViewDidFinishRenderingFrame(_: MLNMapView, fullyRendered _: Bool,
                                        renderingStats: MLNRenderingStats) {
        frames.append((renderingStats.drawableWaitTime, renderingStats.gpuTime))
        counters = (renderingStats.numDrapeTargetsMade, renderingStats.numDrapeTargetsFreed,
                    renderingStats.numDrapeTargetsRendered, renderingStats.numTerrainVertices)
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
        XCTAssertTrue(frames.contains { $0.gpu > 0 }, "no frame reported the GPU's time")
        for frame in frames {
            XCTAssertGreaterThanOrEqual(frame.wait, 0)
            XCTAssertLessThan(frame.wait, 1)
            XCTAssertLessThan(frame.gpu, 1)
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
        let times = frames.map(\.gpu).filter { $0 > 0 }
        XCTAssertGreaterThan(times.count, 20, "too few frames with a GPU time")
        let changes = zip(times, times.dropFirst()).filter { $0.0 != $0.1 }.count
        XCTAssertGreaterThan(Double(changes), Double(times.count) / 2,
                             "the GPU time stayed one value over \(times.count - changes) frames")
    }

    // 197 a tilted terrain map hands the app its drape targets, re-renders and vertices
    func testTerrainCountersReachTheApp() throws {
        try loadMap()
        tilt(60)
        waitUntilFullyRendered()
        XCTAssertGreaterThan(counters.made, 0, "no drape target was counted")
        XCTAssertGreaterThan(counters.drapes, 0, "no drape re-render was counted")
        XCTAssertGreaterThan(counters.vertices, 0, "no terrain vertex was counted")
        XCTAssertLessThanOrEqual(counters.freed, counters.made, "more targets freed than made")
    }
}
