#include <mln/test/util.hpp>

#include <mln/gfx/vertex_vector.hpp>
#include <mln/renderer/layers/hillshade_drawable_current.hpp>
#include <mln/util/monotonic_timer.hpp>

#include <chrono>
#include <cstdint>

using namespace mln;

namespace {
struct Vertex {
    int16_t x, y;
};
} // namespace

// 112 Every relief tile rebuilt its vertices each frame though its bucket had not changed.
TEST(HillshadeDrawableCurrent, AnUnchangedBucketKeepsItsDrawable) {
    gfx::VertexVector<Vertex> vertices;
    vertices.emplace_back(Vertex{0, 0});
    vertices.updateModified();
    const auto uploaded = util::MonotonicTimer::now();

    EXPECT_TRUE(hillshadeDrawableIsCurrent(uploaded, vertices));
}

// 112 A new mask reshapes the bucket, so its drawable is built again.
TEST(HillshadeDrawableCurrent, ANewMaskRebuildsTheDrawable) {
    gfx::VertexVector<Vertex> vertices;
    vertices.clear();
    vertices.updateModified(true);
    const auto uploaded = vertices.getLastModified() - std::chrono::milliseconds(1);

    EXPECT_FALSE(hillshadeDrawableIsCurrent(uploaded, vertices));
    EXPECT_FALSE(hillshadeDrawableIsCurrent(std::nullopt, vertices));
}
