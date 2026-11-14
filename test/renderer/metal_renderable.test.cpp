#if MLN_RENDER_BACKEND_METAL
#include <mln/test/util.hpp>

#include <mln/gfx/backend_scope.hpp>
#include <mln/gfx/headless_backend.hpp>
#include <mln/mtl/renderable_resource.hpp>
#include <mln/mtl/renderer_backend.hpp>

#include <Metal/Metal.hpp>

#include <chrono>
#include <future>

using namespace mln;

// Each frame an upload pass and then a render pass bind the headless renderable, and only the
// swap after the render pass commits. If the second bind replaces the pending command buffer, one
// buffer per frame is never committed, and after about 64 frames (the queue's default pool) the
// next commandBuffer() call blocks forever.
TEST(MetalRenderable, BindKeepsThePendingCommandBuffer) {
    auto backend = gfx::HeadlessBackend::Create({256, 256});
    gfx::BackendScope scope{*backend->getRendererBackend()};
    auto& resource = backend->getRendererBackend()->getDefaultRenderable().getResource<mtl::RenderableResource>();

    resource.bind();
    const MTL::CommandBuffer* pending = resource.getCommandBuffer().get();
    ASSERT_NE(pending, nullptr);

    resource.bind();
    EXPECT_EQ(resource.getCommandBuffer().get(), pending);

    resource.swap();
    resource.bind();
    EXPECT_NE(resource.getCommandBuffer().get(), nullptr);
    resource.swap();
}

// 133: a swap that waited for the GPU held the main thread once per terrain drape.
TEST(MetalRenderable, SwapLeavesTheWaitToTheRead) {
    auto backend = gfx::HeadlessBackend::Create({256, 256});
    gfx::BackendScope scope{*backend->getRendererBackend()};
    auto& renderer = static_cast<mtl::RendererBackend&>(*backend->getRendererBackend());
    auto& resource = renderer.getDefaultRenderable().getResource<mtl::RenderableResource>();

    // Enqueued and not yet committed, it keeps the queue's later buffers from completing.
    auto blocker = NS::RetainPtr(renderer.getCommandQueue()->commandBuffer());
    blocker->enqueue();

    resource.bind();
    auto swapped = std::async(std::launch::async, [&] { resource.swap(); });
    const bool returned = swapped.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
    blocker->commit();
    swapped.get();
    EXPECT_TRUE(returned);

    EXPECT_EQ(backend->readStillImage().size, Size(256, 256));
}
#endif
