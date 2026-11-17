#include <mln/test/util.hpp>
#include <mln/test/stub_file_source.hpp>
#include <mln/test/map_adapter.hpp>

#include <mln/gfx/headless_frontend.hpp>
#include <mln/map/camera.hpp>
#include <mln/map/map_options.hpp>
#include <mln/style/style.hpp>
#include <mln/util/image.hpp>
#include <mln/util/run_loop.hpp>

#include <array>
#include <cstdint>
#include <string>

using namespace mln;

namespace {

constexpr uint32_t side = 256;

std::string polygon(int west, int east) {
    const std::string w = std::to_string(west);
    const std::string e = std::to_string(east);
    return R"({"type": "geojson", "data": {"type": "Polygon", "coordinates": [[[)" + w + R"(, -80], [)" + e +
           R"(, -80], [)" + e + R"(, 80], [)" + w + R"(, 80], [)" + w + R"(, -80]]]}})";
}

std::string fill(const std::string& id, const std::string& source, const std::string& colour, bool visible = true) {
    return R"({"id": ")" + id + R"(", "type": "fill", "source": ")" + source +
           R"(", "layout": {"visibility": ")" + (visible ? "visible" : "none") +
           R"("}, "paint": {"fill-color": ")" + colour + R"(", "fill-antialias": false}})";
}

// Source "a" covers the world and "b" its west, and their layers interleave.
std::string interleavedStyle() {
    return R"({"version": 8, "sources": {"a": )" + polygon(-179, 179) + R"(, "b": )" + polygon(-179, 0) +
           R"(}, "layers": [{"id": "ground", "type": "background", "paint": {"background-color": "#ff0000"}}, )" +
           fill("b-under", "b", "#0000ff") + ", " + fill("a-world", "a", "#00ff00") + ", " +
           fill("b-over", "b", "#ffff00") + ", " + fill("a-hidden", "a", "#ff00ff", false) + "]}";
}

std::array<uint8_t, 4> pixel(const PremultipliedImage& image, uint32_t x, uint32_t y) {
    const uint8_t* p = image.data.get() + (static_cast<size_t>(y) * image.size.width + x) * 4;
    return {p[0], p[1], p[2], p[3]};
}

} // namespace

// 153 createRenderTree walks each source's own layers; the order drawn stays the style's.
TEST(LayersBySource, InterleavedSourcesDrawInTheStylesOrder) {
    util::RunLoop loop;
    auto fileSource = std::make_shared<StubFileSource>(ResourceOptions::Default(), ClientOptions());
    HeadlessFrontend frontend{{side, side}, 1};
    MapAdapter map(frontend,
                   MapObserver::nullObserver(),
                   fileSource,
                   MapOptions().withMapMode(MapMode::Static).withSize(frontend.getSize()));
    map.getStyle().loadJSON(interleavedStyle());
    map.jumpTo(CameraOptions().withCenter(LatLng{0, 0}).withZoom(1));

    const PremultipliedImage image = frontend.render(map).image;
    ASSERT_EQ(image.size.width, side);

    // West: b-over, the last visible layer, over a-world and b-under.
    EXPECT_EQ(pixel(image, side / 4, side / 2), (std::array<uint8_t, 4>{255, 255, 0, 255}));
    // East: b reaches no further, so a-world; the hidden a-hidden draws nothing.
    EXPECT_EQ(pixel(image, side * 3 / 4, side / 2), (std::array<uint8_t, 4>{0, 255, 0, 255}));
}

// 153 the background, which reads no source, is still drawn once, under a source's layer.
TEST(LayersBySource, BackgroundWithoutASourceDrawsBesideASourcesLayer) {
    util::RunLoop loop;
    auto fileSource = std::make_shared<StubFileSource>(ResourceOptions::Default(), ClientOptions());
    HeadlessFrontend frontend{{side, side}, 1};
    MapAdapter map(frontend,
                   MapObserver::nullObserver(),
                   fileSource,
                   MapOptions().withMapMode(MapMode::Static).withSize(frontend.getSize()));
    map.getStyle().loadJSON(
        R"({"version": 8, "sources": {"a": )" + polygon(-179, 0) +
        R"(}, "layers": [{"id": "ground", "type": "background", "paint": {"background-color": "#ff0000"}}, )" +
        fill("a-west", "a", "#00ff00") + "]}");
    map.jumpTo(CameraOptions().withCenter(LatLng{0, 0}).withZoom(1));

    const PremultipliedImage image = frontend.render(map).image;
    EXPECT_EQ(pixel(image, side / 4, side / 2), (std::array<uint8_t, 4>{0, 255, 0, 255}));
    EXPECT_EQ(pixel(image, side * 3 / 4, side / 2), (std::array<uint8_t, 4>{255, 0, 0, 255}));
}
