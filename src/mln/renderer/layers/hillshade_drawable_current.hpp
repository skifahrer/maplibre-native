#pragma once

#include <mln/gfx/vertex_vector.hpp>

#include <chrono>
#include <optional>

namespace mln {

/// Whether a relief tile's drawable was uploaded after its bucket's geometry last changed.
inline bool hillshadeDrawableIsCurrent(std::optional<std::chrono::duration<double>> uploaded,
                                       const gfx::VertexVectorBase& bucketVertices) {
    return uploaded && !bucketVertices.isModifiedAfter(*uploaded);
}

} // namespace mln
