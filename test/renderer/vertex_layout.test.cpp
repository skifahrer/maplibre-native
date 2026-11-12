#include <mln/test/util.hpp>

#include <mln/gfx/vertex_layout.hpp>

using namespace mln;
using namespace mln::gfx;

namespace {
const auto* const someBuffer = reinterpret_cast<const VertexBufferResource*>(0x10);
const auto* const otherBuffer = reinterpret_cast<const VertexBufferResource*>(0x20);

std::vector<std::optional<AttributeBinding>> bindings(const VertexBufferResource* buffer) {
    return {AttributeBinding{{AttributeDataType::Short2, 0}, 4, buffer, 0, 0},
            std::nullopt,
            AttributeBinding{{AttributeDataType::Float, 0}, 4, buffer, 0, 2}};
}
} // namespace

// 111 Every new frame of symbol vertices rebuilt a Metal vertex descriptor of the same layout.
TEST(VertexLayout, NewVertexDataKeepsTheLayout) {
    EXPECT_EQ(vertexLayoutHash(bindings(someBuffer), {}), vertexLayoutHash(bindings(otherBuffer), {}));
}

// 111 A layout that does change still builds a new descriptor.
TEST(VertexLayout, AChangedLayoutIsNew) {
    const auto before = vertexLayoutHash(bindings(someBuffer), {});

    auto offset = bindings(someBuffer);
    offset[2]->attribute.offset = 4;
    EXPECT_NE(before, vertexLayoutHash(offset, {}));

    auto type = bindings(someBuffer);
    type[0]->attribute.dataType = AttributeDataType::Float2;
    EXPECT_NE(before, vertexLayoutHash(type, {}));

    auto stride = bindings(someBuffer);
    stride[0]->vertexStride = 8;
    EXPECT_NE(before, vertexLayoutHash(stride, {}));

    auto slot = bindings(someBuffer);
    slot[2]->bufferIndex = 3;
    EXPECT_NE(before, vertexLayoutHash(slot, {}));

    EXPECT_NE(before, vertexLayoutHash(bindings(nullptr), {}));
    EXPECT_NE(before, vertexLayoutHash({}, bindings(someBuffer)));
}
