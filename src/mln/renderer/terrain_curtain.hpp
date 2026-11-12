#pragma once

#include <mln/tile/tile_id.hpp>

#include <cmath>
#include <cstdint>
#include <optional>

namespace mln {
namespace terrain {

/// Bits of the tile edges that hang no curtain: left, right, top, bottom.
enum CurtainlessEdge : uint8_t {
    LeftEdge = 1,
    RightEdge = 2,
    TopEdge = 4,
    BottomEdge = 8
};

/// The edges of `tile` with no ground just past them, by `groundAt` in world units.
template <typename GroundAt>
uint8_t curtainlessEdges(const CanonicalTileID& tile, const GroundAt& groundAt) {
    // Every mesh column's step, ends too: a short gap between samples kept its curtain
    constexpr int samples = 129;
    const double size = 1.0 / static_cast<double>(1u << tile.z);
    const double x0 = tile.x * size;
    const double y0 = tile.y * size;
    // A thousandth of the tile past the edge lies in the neighbour, whatever its zoom
    const double out = size / 1024.0;
    const auto bare = [&](double x, double y) {
        const std::optional<bool> ground = groundAt(x, y);
        return ground && !*ground;
    };
    uint8_t edges = 0;
    for (int i = 0; i < samples; ++i) {
        const double along = out + i / static_cast<double>(samples - 1) * (size - 2 * out);
        if (bare(x0 - out, y0 + along)) edges |= LeftEdge;
        if (bare(x0 + size + out, y0 + along)) edges |= RightEdge;
        if (bare(x0 + along, y0 - out)) edges |= TopEdge;
        if (bare(x0 + along, y0 + size + out)) edges |= BottomEdge;
    }
    return edges;
}

/// The DEM size with the curtainless edges in its fraction, for the Metal shaders.
inline float withCurtainlessEdges(float demDim, uint8_t edges) {
    return std::floor(demDim) + static_cast<float>(edges & 15) / 16.0f;
}

} // namespace terrain
} // namespace mln
