#pragma once

#include <mln/map/mode.hpp>

#include <cctype>
#include <cstddef>
#include <cstdlib>

namespace mln {

/// The texels all drapes may hold; `forced`, in millions, stands in for the mode's own.
inline std::size_t drapeTexelBudget(TerrainLoadMode mode, const char* forced) {
    if (forced && std::isdigit(static_cast<unsigned char>(forced[0]))) {
        char* end = nullptr;
        const unsigned long millions = std::strtoul(forced, &end, 10);
        if (end != forced && *end == '\0' && millions > 0) {
            return static_cast<std::size_t>(millions) << 20;
        }
    }
    return terrainLoadBudget(mode).drapeTexels;
}

/// 203 Whether a drape of a tile at `tileZoom` is far: `levels` or more below the nearest tile's.
inline bool isFarDrape(uint8_t tileZoom, uint8_t nearestZoom, uint8_t levels) {
    return levels > 0 && tileZoom + levels <= nearestZoom;
}

/// 158: `MLN_DRAPE_MEGATEXELS=32` sets it for a run, to weigh memory against sharpness.
inline std::size_t drapeTexelBudget(TerrainLoadMode mode) {
    static const char* const forced = std::getenv("MLN_DRAPE_MEGATEXELS");
    return drapeTexelBudget(mode, forced);
}

} // namespace mln
