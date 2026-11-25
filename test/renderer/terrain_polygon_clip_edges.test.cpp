#include <mln/test/util.hpp>
#include <mln/test/stub_file_source.hpp>
#include <mln/test/map_adapter.hpp>

#include <mln/gfx/headless_frontend.hpp>
#include <mln/map/camera.hpp>
#include <mln/map/map_options.hpp>
#include <mln/renderer/buckets/line_bucket.hpp>
#include <mln/style/style.hpp>
#include <mln/util/image.hpp>
#include <mln/util/run_loop.hpp>

#include <string>

using namespace mln;

namespace {

// A park far wider than the view, outlined in red 32 px wide from tiles with a 2 px buffer, so a
// drawn cut edge reaches 14 px into each tile it borders.
std::string parkStyle(bool terrain) {
    const std::string terrainEntry = terrain ? R"("terrain": {"source": "dem", "exaggeration": 1.0},)" : "";
    return R"({
  "version": 8,
  "sources": {
    "dem": {
      "type": "raster-dem",
      "tiles": ["http://example.com/dem/{z}-{x}-{y}.png"],
      "encoding": "mapbox",
      "maxzoom": 11,
      "tileSize": 256
    },
    "park": {
      "type": "geojson",
      "maxzoom": 13,
      "buffer": 2,
      "data": {
        "type": "Feature",
        "properties": {},
        "geometry": {"type": "Polygon", "coordinates": [[[9, 45], [11, 45], [11, 47], [9, 47], [9, 45]]]}
      }
    }
  },)" + terrainEntry +
           R"(
  "layers": [{"id": "park", "type": "line", "source": "park",
              "paint": {"line-width": 32, "line-color": "red"}}]
})";
}

std::string seaLevelDEMTile() {
    PremultipliedImage image({256, 256});
    for (uint32_t i = 0; i < 256 * 256; ++i) {
        uint8_t* px = image.data.get() + i * 4;
        px[0] = 1;
        px[1] = 134;
        px[2] = 160;
        px[3] = 255;
    }
    return encodePNG(image);
}

// Red pixels in a 512 px view at zoom 13, where 512 px tiles always leave a border in sight.
int redPixels(bool terrain, LatLng center = LatLng{46.0, 10.0}) {
    util::RunLoop loop;
    auto fileSource = std::make_shared<StubFileSource>(ResourceOptions::Default(), ClientOptions());
    const std::string dem = seaLevelDEMTile();
    fileSource->tileResponse = [&](const Resource&) {
        Response res;
        res.data = std::make_shared<std::string>(dem);
        return res;
    };
    HeadlessFrontend frontend{{512, 512}, 1};
    MapAdapter map(frontend,
                   MapObserver::nullObserver(),
                   fileSource,
                   MapOptions().withMapMode(MapMode::Static).withSize(frontend.getSize()));
    map.getStyle().loadJSON(parkStyle(terrain));
    map.jumpTo(CameraOptions().withCenter(center).withZoom(13.0).withPitch(0.0));

    PremultipliedImage image;
    for (int i = 0; i < 4; ++i) {
        loop.runOnce();
        image = frontend.render(map).image;
    }

    int red = 0;
    for (uint32_t i = 0; i < image.size.width * image.size.height; ++i) {
        const uint8_t* px = image.data.get() + i * 4;
        red += px[0] > 100 && px[1] < 90 && px[2] < 90 ? 1 : 0;
    }
    return red;
}

} // namespace

// 178 Riki Maps' ACxS: a protected area's outline showed as thick bands along 3D tile borders
TEST(TerrainDrape, PolygonOutlinesDrawNoTileCutEdges) {
    ASSERT_GT(redPixels(false, LatLng{46.0, 9.0}), 1000) << "the park's own border";
    EXPECT_EQ(0, redPixels(false)) << "flat";
    EXPECT_EQ(0, redPixels(true)) << "draped";
}

// 178
TEST(TerrainDrape, OutlineKeepsABoundaryCrossingTheTile) {
    const GeometryCoordinates ring{{-64, -64}, {4000, -64}, {4000, 3000}, {-64, 3000}, {-64, -64}};
    const auto runs = LineBucket::drawnRuns(ring);
    ASSERT_TRUE(runs);
    ASSERT_EQ(1u, runs->size());
    EXPECT_EQ((GeometryCoordinates{{4000, -64}, {4000, 3000}, {-64, 3000}}), runs->front());
}

// 178
TEST(TerrainDrape, OutlineSplitsAtEachCutEdge) {
    const GeometryCoordinates ring{{-64, 100}, {-64, 50}, {500, 50}, {500, 8256}, {600, 8256}, {600, 100}};
    const auto runs = LineBucket::drawnRuns(ring);
    ASSERT_TRUE(runs);
    ASSERT_EQ(2u, runs->size());
    EXPECT_EQ((GeometryCoordinates{{-64, 50}, {500, 50}, {500, 8256}}), (*runs)[0]);
    EXPECT_EQ((GeometryCoordinates{{600, 8256}, {600, 100}, {-64, 100}}), (*runs)[1]);
}

// 178
TEST(TerrainDrape, OutlineOfAnUncutRingIsWhole) {
    EXPECT_FALSE(LineBucket::drawnRuns({{10, 10}, {500, 10}, {500, 500}, {10, 10}}));
}
