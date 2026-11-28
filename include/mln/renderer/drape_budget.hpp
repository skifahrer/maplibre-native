#pragma once

#include <mln/map/mode.hpp>

#include <cctype>
#include <cstddef>
#include <cstdint>
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

/// 203 The far drapes' levels: the map's, or `forced` while the map leaves them at 0.
inline uint8_t farDrapeLevels(uint8_t set, const char* forced) {
    if (set == 0 && forced && std::isdigit(static_cast<unsigned char>(forced[0]))) {
        char* end = nullptr;
        const unsigned long levels = std::strtoul(forced, &end, 10);
        if (end != forced && *end == '\0' && levels <= 22) {
            return static_cast<uint8_t>(levels);
        }
    }
    return set;
}

/// 203: `MLN_FAR_DRAPE_LEVELS=2` sets them for a run, to measure them before the app does.
inline uint8_t farDrapeLevels(uint8_t set) {
    static const char* const forced = std::getenv("MLN_FAR_DRAPE_LEVELS");
    return farDrapeLevels(set, forced);
}

/// 158: `MLN_DRAPE_MEGATEXELS=32` sets it for a run, to weigh memory against sharpness.
inline std::size_t drapeTexelBudget(TerrainLoadMode mode) {
    static const char* const forced = std::getenv("MLN_DRAPE_MEGATEXELS");
    return drapeTexelBudget(mode, forced);
}

} // namespace mln
