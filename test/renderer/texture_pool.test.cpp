#include <mln/test/util.hpp>
#include <mln/gfx/backend_scope.hpp>
#include <mln/gfx/context.hpp>
#include <mln/gfx/headless_backend.hpp>
#include <mln/gfx/rendering_stats.hpp>
#include <mln/gfx/texture2d.hpp>
#include <mln/renderer/texture_pool.hpp>
#include <mln/tile/tile_id.hpp>

using namespace mln;

namespace {
class DrapePool : public ::testing::Test {
protected:
    std::unique_ptr<gfx::HeadlessBackend> backend = gfx::HeadlessBackend::Create({64, 64});
    gfx::BackendScope scope{*backend->getRendererBackend()};
    gfx::Context& context = backend->getRendererBackend()->getContext();
    TexturePool pool{64};
    const UnwrappedTileID a{14, 9000, 5600};
    const UnwrappedTileID b{14, 9001, 5600};
    const UnwrappedTileID c{14, 9002, 5600};
    static constexpr std::size_t tile = 64 * 64;

    void show(const std::set<UnwrappedTileID>& tiles, std::size_t keptTexels) {
        for (const auto& id : tiles) {
            pool.createRenderTarget(context, id, Color::white());
        }
        freed += pool.removeStaleRenderTargets(tiles, keptTexels);
    }
    std::size_t freed = 0;
};
} // namespace

// 202 2CA1 a tile that leaves and comes back takes the same target, not a new one
TEST_F(DrapePool, ATileComingBackTakesItsTarget) {
    show({a, b}, 4 * tile);
    const auto first = pool.getRenderTarget(a);
    const auto made = context.renderingStats().numDrapeTargetsMade;

    show({b}, 4 * tile);
    EXPECT_EQ(pool.getRenderTarget(a), nullptr) << "a departed target is still drawn";
    EXPECT_EQ(pool.keptCount(), 1u);
    EXPECT_EQ(freed, 0u);

    show({a, b}, 4 * tile);
    EXPECT_EQ(pool.getRenderTarget(a), first);
    EXPECT_EQ(context.renderingStats().numDrapeTargetsMade, made) << "the returning tile made a target";
    EXPECT_EQ(pool.keptCount(), 0u);
}

// 202 2CA1 kept targets fit the texels left to them, the earliest to leave going first
TEST_F(DrapePool, TheEarliestToLeaveGoFirst) {
    show({a, b, c}, 0);
    show({b, c}, 2 * tile);
    show({c}, 2 * tile);
    EXPECT_EQ(pool.keptCount(), 2u);
    EXPECT_EQ(freed, 0u);

    show({c}, tile);
    EXPECT_EQ(pool.keptCount(), 1u);
    EXPECT_EQ(freed, 1u);
    const auto made = context.renderingStats().numDrapeTargetsMade;
    show({b, c}, tile);
    EXPECT_EQ(context.renderingStats().numDrapeTargetsMade, made) << "b, the later to leave, was not kept";
}

// 202 2CA1 no texels left frees every departed and kept target, as turning terrain off does
TEST_F(DrapePool, NoTexelsLeftFreesThemAll) {
    show({a, b}, 4 * tile);
    show({b}, 4 * tile);
    show({}, 0);
    EXPECT_EQ(pool.keptCount(), 0u);
    EXPECT_EQ(freed, 2u);
    EXPECT_EQ(pool.retiredCount(), 2u);
}

// 202 2CA1 a returning tile wanting another size gets a new target and the kept one is freed
TEST_F(DrapePool, AReturningTileOfAnotherSizeIsRemade) {
    show({a}, 4 * tile);
    const auto first = pool.getRenderTarget(a);
    show({}, 4 * tile);
    const auto freedBefore = context.renderingStats().numDrapeTargetsFreed;
    pool.createRenderTarget(context, a, Color::white(), 128);
    EXPECT_NE(pool.getRenderTarget(a), first);
    EXPECT_EQ(pool.getRenderTarget(a)->getTexture()->getSize().width, 128u);
    EXPECT_EQ(context.renderingStats().numDrapeTargetsFreed, freedBefore + 1);
    EXPECT_EQ(pool.keptCount(), 0u);
}

// 202 2CA1 a kept target's side counts as the tile's own, so coming back does not regrow it
TEST_F(DrapePool, AKeptTargetsSideCounts) {
    show({a}, 4 * tile);
    show({}, 4 * tile);
    EXPECT_EQ(pool.sideOf(a), 64u);
    EXPECT_EQ(pool.sideOf(b), 0u);
}
