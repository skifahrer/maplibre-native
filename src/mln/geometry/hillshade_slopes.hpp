#pragma once

#include <mln/util/image.hpp>

#include <cstdint>

namespace mln {

class DEMData;

/// The slope image the hillshade prepare shader draws, made on the CPU from a tile's heights.
class HillshadeSlopes {
public:
    /// Pixel (i, j) holds the slope at height pixel (i - 1, j - 1): the tile and a one-pixel ring.
    static PremultipliedImage make(const DEMData& dem, uint8_t zoom);

    /// Redraws the two outer pixels on every side, the ones a backfilled border changes.
    static void remakeEdges(PremultipliedImage& slopes, const DEMData& dem, uint8_t zoom);
};

} // namespace mln
