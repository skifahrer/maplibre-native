#include <mln/test/util.hpp>

#include <mln/renderer/terrain_curtain.hpp>

#if MLN_RENDER_BACKEND_METAL
#include <mln/shaders/mtl/common.hpp>
#include <mln/shaders/mtl/terrain.hpp>
#include <mln/shaders/mtl/terrain_depth.hpp>

#include <string>
#endif

#include <optional>

using namespace mln;
using namespace mln::terrain;

namespace {
const CanonicalTileID tile{13, 4552, 2807};
const double size = 1.0 / 8192.0;
const double east = (4552 + 1) * size;
} // namespace

// 117 Ground all round: every edge hangs its curtain, as before.
TEST(TerrainCurtain, GroundAllRoundHangsEveryCurtain) {
    EXPECT_EQ(0, curtainlessEdges(tile, [](double, double) { return std::optional<bool>(true); }));
}

// 117 At the Tatras border a curtain hung into the void past its edge, a spike.
TEST(TerrainCurtain, NoGroundPastAnEdgeHangsNoCurtainThere) {
    const auto polandEast = [](double x, double) {
        return std::optional<bool>(x < east);
    };
    EXPECT_EQ(RightEdge, curtainlessEdges(tile, polandEast));
}

// 117 A short gap near a corner, between the old nine samples, still kept its curtain.
TEST(TerrainCurtain, AGapNearACornerIsFound) {
    const double top = 2807 * size;
    const auto gap = [&](double x, double y) {
        return std::optional<bool>(!(x >= east && y < top + size / 100));
    };
    EXPECT_EQ(RightEdge, curtainlessEdges(tile, gap) & RightEdge);
}

// 117 Where no height model is loaded nothing is known, so the curtain stays.
TEST(TerrainCurtain, UnknownGroundKeepsTheCurtain) {
    EXPECT_EQ(0, curtainlessEdges(tile, [](double, double) { return std::optional<bool>(); }));
}

// 117 The edges ride in the DEM size's fraction and leave the size whole.
TEST(TerrainCurtain, TheEdgesRideInTheFraction) {
    const float packed = withCurtainlessEdges(256.0f, RightEdge | BottomEdge);
    EXPECT_EQ(256.0f, std::floor(packed));
    EXPECT_EQ(RightEdge | BottomEdge, static_cast<int>(std::lround((packed - std::floor(packed)) * 16.0f)));
    EXPECT_EQ(512.0f, withCurtainlessEdges(512.0f, 0));
}

#if MLN_RENDER_BACKEND_METAL
// 117 Both Metal terrain passes read the size whole and skip flagged edges.
// 122, 125 Only Metal builds this, and under -Werror.
TEST(TerrainCurtain, TheMetalShadersReadTheEdges) {
    EXPECT_NE(std::string::npos, std::string(shaders::prelude).find("inline bool curtain_hangs("));
    using Terrain = shaders::ShaderSource<shaders::BuiltIn::TerrainShader, gfx::Backend::Type::Metal>;
    using Depth = shaders::ShaderSource<shaders::BuiltIn::TerrainDepthShader, gfx::Backend::Type::Metal>;
    for (const std::string& source : {std::string(Terrain::source), std::string(Depth::source)}) {
        EXPECT_NE(std::string::npos, source.find("floor(drawable.dem_coords.w)"));
        EXPECT_NE(std::string::npos, source.find("curtain_hangs(pos, drawable.dem_coords.w)"));
    }
}
#endif
