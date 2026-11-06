#if MLN_RENDER_BACKEND_METAL
#include <mln/test/util.hpp>

#include <mln/gfx/backend_scope.hpp>
#include <mln/gfx/command_encoder.hpp>
#include <mln/gfx/context.hpp>
#include <mln/gfx/headless_backend.hpp>
#include <mln/gfx/offscreen_texture.hpp>
#include <mln/gfx/render_pass.hpp>

#include <new>

using namespace mln;

// 82 RenderTarget::render catches what this throws; uncaught, it aborted the app.
TEST(MetalOffscreen, AnUnallocatableTargetThrowsBadAlloc) {
    auto backend = gfx::HeadlessBackend::Create({64, 64});
    gfx::BackendScope scope{*backend->getRendererBackend()};
    auto& context = backend->getRendererBackend()->getContext();
    auto encoder = context.createCommandEncoder();

    auto fits = context.createOffscreenTexture({64, 64}, gfx::TextureChannelDataType::UnsignedByte);
    EXPECT_NO_THROW(encoder->createRenderPass("fits", {.renderable = *fits, .clearColor = Color{}}));

    // Past any Metal device's largest texture side, so the device hands back no texture.
    auto tooBig = context.createOffscreenTexture({65536, 65536}, gfx::TextureChannelDataType::UnsignedByte);
    EXPECT_THROW(encoder->createRenderPass("too big", {.renderable = *tooBig, .clearColor = Color{}}),
                 std::bad_alloc);
}
#endif
