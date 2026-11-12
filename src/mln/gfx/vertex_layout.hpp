#pragma once

#include <mln/gfx/attribute.hpp>
#include <mln/util/hash.hpp>

#include <cstddef>
#include <optional>
#include <vector>

namespace mln {
namespace gfx {

/// Hashes what a vertex layout is built from, leaving out which buffers hold the vertices.
inline std::size_t vertexLayoutHash(const std::vector<std::optional<AttributeBinding>>& perVertex,
                                    const std::vector<std::optional<AttributeBinding>>& perInstance) {
    std::size_t hash{0};
    const auto combine = [&](const std::vector<std::optional<AttributeBinding>>& bindings, bool instanced) {
        std::size_t index = 0;
        for (const auto& binding : bindings) {
            if (binding) {
                util::hash_combine(hash,
                                   util::hash(index,
                                              instanced,
                                              binding->attribute.offset,
                                              binding->attribute.dataType,
                                              binding->vertexStride,
                                              binding->bufferIndex,
                                              binding->vertexBufferResource != nullptr));
            }
            index += 1;
        }
    };
    combine(perVertex, false);
    combine(perInstance, true);
    return hash;
}

} // namespace gfx
} // namespace mln
