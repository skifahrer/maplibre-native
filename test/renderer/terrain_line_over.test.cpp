#include <mln/test/util.hpp>
#include <mln/test/stub_file_source.hpp>
#include <mln/test/map_adapter.hpp>

#include <mln/gfx/headless_frontend.hpp>
#include <mln/map/camera.hpp>
#include <mln/map/map_options.hpp>
#include <mln/style/layer.hpp>
#include <mln/style/style.hpp>
#include <mln/util/image.hpp>
#include <mln/util/run_loop.hpp>

#include <memory>
#include <string>

using namespace mln;

namespace {

// A red line north-south a little west of the center, under a tall blue building that covers it,
// on flat ground at sea level, seen straight down at zoom 16.
std::string routeUnderABuilding(const std::string& linePaint) {
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
    "line": {
      "type": "geojson",
      "maxzoom": 16,
      "data": {"type": "LineString", "coordinates": [[9.9995, 45.99], [9.9995, 46.01]]}
    },
    "building": {
      "type": "geojson",
      "maxzoom": 16,
      "data": {"type": "Polygon", "coordinates": [[[9.9990, 45.9995], [10.0000, 45.9995],
                                                   [10.0000, 46.0005], [9.9990, 46.0005], [9.9990, 45.9995]]]}
    }
  },
  "terrain": {"source": "dem", "exaggeration": 1.0},
  "layers": [
    {"id": "building", "type": "fill-extrusion", "source": "building",
     "paint": {"fill-extrusion-color": "blue", "fill-extrusion-height": 200}},
    {"id": "line", "type": "line", "source": "line",
     "paint": {"line-width": 16, "line-color": "red")" +
           linePaint + R"(}}
  ]
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

// Red pixels in the band where the line runs, inside the building's square
int redOverTheBuilding(bool drawnOver, const std::string& linePaint = "") {
    util::RunLoop loop;
    auto fileSource = std::make_shared<StubFileSource>(ResourceOptions::Default(), ClientOptions());
    const std::string dem = seaLevelDEMTile();
    fileSource->tileResponse = [&](const Resource&) {
        Response res;
        res.data = std::make_shared<std::string>(dem);
        return res;
    };
    HeadlessFrontend frontend{{256, 256}, 1};
    MapAdapter map(frontend,
                   MapObserver::nullObserver(),
                   fileSource,
                   MapOptions().withMapMode(MapMode::Static).withSize(frontend.getSize()));
    map.getStyle().loadJSON(routeUnderABuilding(linePaint));
    map.getStyle().getLayer("line")->setDrawnOverTerrain(drawnOver);
    map.jumpTo(CameraOptions().withCenter(LatLng{46.0, 10.0}).withZoom(16.0).withPitch(0.0));

    PremultipliedImage image;
    for (int i = 0; i < 4; ++i) {
        loop.runOnce();
        image = frontend.render(map).image;
    }

    int red = 0;
    for (uint32_t y = 100; y < 156; ++y) {
        for (uint32_t x = 70; x < 92; ++x) {
            const uint8_t* px = image.data.get() + (y * image.size.width + x) * 4;
            red += px[0] > 150 && px[2] < 90 ? 1 : 0;
        }
    }
    return red;
}

} // namespace

// 170 AI8x: under terrain a route was draped, so the buildings it passed covered it
TEST(TerrainLineOver, ARouteDrawnOverTerrainShowsOverBuildings) {
#if !MLN_RENDER_BACKEND_METAL
    GTEST_SKIP() << "only Metal's line shader reads the height model";
#endif
    EXPECT_LT(redOverTheBuilding(false), 50) << "the building no longer covers a draped line";
    EXPECT_GT(redOverTheBuilding(true), 500);
}

// 170 only the plain line shader reads the height model; a dashed line stays draped
TEST(TerrainLineOver, ADashedLineStaysDraped) {
#if !MLN_RENDER_BACKEND_METAL
    GTEST_SKIP() << "only Metal's line shader reads the height model";
#endif
    EXPECT_LT(redOverTheBuilding(true, R"(, "line-dasharray": [2, 1])"), 50);
}
