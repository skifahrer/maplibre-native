#include <mln/test/util.hpp>

#include <mln/gfx/backend_scope.hpp>
#include <mln/gfx/context.hpp>
#include <mln/gfx/headless_backend.hpp>
#include <mln/gfx/texture2d.hpp>
#include <mln/renderer/texture_pool.hpp>

using namespace mln;

namespace {

const UnwrappedTileID left{14, 9000, 5700};
const UnwrappedTileID middle{14, 9001, 5700};
const UnwrappedTileID right{14, 9002, 5700};

struct Pool {
    std::unique_ptr<gfx::HeadlessBackend> backend = gfx::HeadlessBackend::Create({64, 64});
    gfx::BackendScope scope{*backend->getRendererBackend()};
    gfx::Context& context = backend->getRendererBackend()->getContext();
    TexturePool pool{256};

    // As the renderer does each frame: the cover's targets first, then the rest let go.
    void frame(const std::set<UnwrappedTileID>& cover, uint32_t side = 256) {
        for (const auto& id : cover) {
            pool.createRenderTarget(context, id, Color{}, side);
        }
        pool.removeStaleRenderTargets(cover);
    }

    std::weak_ptr<gfx::Texture2D> texture(const UnwrappedTileID& id) const {
        return pool.getRenderTarget(id)->getTexture();
    }

    bool sameTexture(const std::weak_ptr<gfx::Texture2D>& old, const UnwrappedTileID& id) const {
        return !old.expired() && old.lock() == pool.getRenderTarget(id)->getTexture();
    }
};

} // namespace

// 84 a drape that leaves the view hands its texture to one that arrives, not a new allocation
TEST(TexturePool, ATileArrivingTakesTheTextureOfOneThatLeft) {
    Pool test;
    test.frame({left});
    const auto leftTexture = test.texture(left);
    test.frame({middle});
    test.frame({right});
    EXPECT_TRUE(test.sameTexture(leftTexture, right));
    EXPECT_FALSE(test.sameTexture(test.texture(middle), right));
}

// 84 a texture of another size is never handed over
TEST(TexturePool, OnlyATextureOfTheSameSideIsTaken) {
    Pool test;
    test.frame({left});
    const auto leftTexture = test.texture(left);
    test.frame({middle});
    test.frame({right}, 512);
    EXPECT_FALSE(test.sameTexture(leftTexture, right));
    EXPECT_EQ(512u, test.pool.getRenderTarget(right)->getTexture()->getSize().width);
}

// 84 what no tile took within a frame is freed, so memory stays near the budget
TEST(TexturePool, ATextureNobodyTookIsFreedAFrameLater) {
    Pool test;
    test.frame({left});
    const auto leftTexture = test.texture(left);
    test.frame({middle});
    EXPECT_FALSE(leftTexture.expired());
    test.frame({middle});
    EXPECT_TRUE(leftTexture.expired());
}

// 84 a target something still draws with keeps its texture
TEST(TexturePool, ATargetStillHeldElsewhereKeepsItsTexture) {
    Pool test;
    test.frame({left});
    const auto held = test.pool.getRenderTarget(left);
    test.frame({middle});
    test.frame({right});
    ASSERT_NE(nullptr, held->getTexture());
    EXPECT_NE(held->getTexture(), test.pool.getRenderTarget(right)->getTexture());
}

// 84 terrain switched off or memory reclaimed frees every texture at once
TEST(TexturePool, AnEmptyCoverFreesEverything) {
    Pool test;
    test.frame({left});
    const auto leftTexture = test.texture(left);
    test.frame({middle});
    test.pool.removeStaleRenderTargets({});
    EXPECT_TRUE(leftTexture.expired());
}

// 84 a zoom step retires the whole cover; what is over the cap is freed at once, as before
TEST(TexturePool, TexturesPastTheCapAreFreedAtOnce) {
    Pool test;
    test.frame({left, middle}, 4096);
    const auto leftTexture = test.texture(left);
    const auto middleTexture = test.texture(middle);
    test.frame({right}, 256);
    EXPECT_FALSE(leftTexture.expired());
    EXPECT_TRUE(middleTexture.expired());
}
