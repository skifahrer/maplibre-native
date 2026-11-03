#include <mln/map/transform.hpp>
#include <mln/util/color.hpp>
#include <mln/util/geo.hpp>
#include <mln/util/tile_coordinate.hpp>
#include <mln/util/tile_cover.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

using namespace mln;

namespace {

const LatLng tatras{49.18, 20.08};
constexpr uint8_t zoom = 14;

std::vector<OverscaledTileID> tiltedCover(double drawDistance) {
    Transform transform;
    transform.resize({512, 512});
    transform.jumpTo(CameraOptions().withCenter(tatras).withZoom(zoom).withPitch(70.0));
    transform.setDrawDistance(drawDistance);
    return util::tileCover({.transformState = transform.getState()},
                           zoom,
                           Range<uint8_t>{0, zoom},
                           std::optional<uint8_t>{zoom});
}

} // namespace

TEST(TerrainDrawDistance, NegativeOrNotANumberTurnsItOff) {
    TransformState state;
    EXPECT_EQ(state.getDrawDistance(), 0.0);
    state.setDrawDistance(5000.0);
    EXPECT_EQ(state.getDrawDistance(), 5000.0);
    state.setDrawDistance(-1.0);
    EXPECT_EQ(state.getDrawDistance(), 0.0);
    state.setDrawDistance(std::numeric_limits<double>::quiet_NaN());
    EXPECT_EQ(state.getDrawDistance(), 0.0);
    state.setDrawDistance(std::numeric_limits<double>::infinity());
    EXPECT_EQ(state.getDrawDistance(), 0.0);
}

// The far plane moves in, so a steep view loads fewer tiles towards the horizon.
TEST(TerrainDrawDistance, ASteepViewCoversFewerTiles) {
    const auto full = tiltedCover(0.0);
    const auto near = tiltedCover(2000.0);
    ASSERT_FALSE(full.empty());
    EXPECT_FALSE(near.empty());
    EXPECT_LT(near.size(), full.size());
}

// Zooming out with a short draw distance still shows the map under the centre.
TEST(TerrainDrawDistance, NeverCutsNearerThanTheCentre) {
    const auto ids = tiltedCover(1.0);
    ASSERT_FALSE(ids.empty());
    const auto centre = TileCoordinate::fromLatLng(zoom, tatras).p;
    const auto x = static_cast<uint32_t>(std::floor(centre.x));
    const auto y = static_cast<uint32_t>(std::floor(centre.y));
    const bool covered = std::any_of(ids.begin(), ids.end(), [&](const OverscaledTileID& id) {
        return id.canonical.z == zoom && id.canonical.x == x && id.canonical.y == y;
    });
    EXPECT_TRUE(covered);
}

TEST(TerrainFog, IsOffUntilGivenAColour) {
    TransformState state;
    EXPECT_EQ(state.getFogColor().a, 0.0f);
    state.setFog(Color{0.8f, 0.85f, 0.9f, 1.0f}, 5000.0, 20000.0);
    EXPECT_EQ(state.getFogColor(), (Color{0.8f, 0.85f, 0.9f, 1.0f}));
    EXPECT_EQ(state.getFogStart(), 5000.0);
    EXPECT_EQ(state.getFogEnd(), 20000.0);
}

TEST(TerrainFog, DistancesAreNeverNegativeNorEndBeforeTheStart) {
    TransformState state;
    const Color haze{0.8f, 0.85f, 0.9f, 1.0f};
    state.setFog(haze, -100.0, std::numeric_limits<double>::quiet_NaN());
    EXPECT_EQ(state.getFogStart(), 0.0);
    EXPECT_EQ(state.getFogEnd(), 0.0);
    state.setFog(haze, 8000.0, 3000.0);
    EXPECT_EQ(state.getFogStart(), 8000.0);
    EXPECT_EQ(state.getFogEnd(), 8000.0);
}

TEST(TerrainClay, KeepsTheBuiltInClayUntilGivenAColour) {
    TransformState state;
    EXPECT_EQ(state.getClayColor().a, 0.0f);
    state.setClayColor(Color{0.3f, 0.25f, 0.2f, 1.0f});
    EXPECT_EQ(state.getClayColor(), (Color{0.3f, 0.25f, 0.2f, 1.0f}));
    state.setClayColor(Color{0.0f, 0.0f, 0.0f, 0.0f});
    EXPECT_EQ(state.getClayColor().a, 0.0f);
}

// The clay and fog reach the renderer through the transform, so a Transform keeps them too.
TEST(TerrainFog, TheTransformPassesFogAndClayToItsState) {
    Transform transform;
    transform.setFog(Color{0.8f, 0.85f, 0.9f, 1.0f}, 1000.0, 9000.0);
    transform.setClayColor(Color{0.3f, 0.25f, 0.2f, 1.0f});
    EXPECT_EQ(transform.getState().getFogEnd(), 9000.0);
    EXPECT_EQ(transform.getState().getClayColor(), (Color{0.3f, 0.25f, 0.2f, 1.0f}));
}
