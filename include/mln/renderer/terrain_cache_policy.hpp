#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <vector>

namespace mln {
namespace terrain {

/// The DEM a terrain mesh drawable was built on.
struct DEMBinding {
    /// 0 = flat placeholder, 1 = closest loaded ancestor DEM, 2 = the tile's own DEM
    uint8_t tier = 0;
    /// Zoom of the bound DEM tile (-1 for the placeholder)
    int8_t demZoom = -1;
};

/// Whether a terrain drawable built on `current` should be rebuilt now that `available` could be
/// bound instead. A better tier always wins. Within the ancestor tier a deeper ancestor wins as
/// well: a fresh wide view often builds its tiles on whatever coarse ancestor is loaded first (z6
/// under a z13 tile samples 1/128th of that DEM, which renders flat), and the deeper ancestors that
/// load moments later must replace it. maplibre-gl-js resolves the covering DEM tile every frame
/// (Terrain.getTerrainData -> TileManager.getSourceTile(tileID, true)), so it never keeps a
/// shallower one than it has.
inline bool shouldRebind(const DEMBinding& current, const DEMBinding& available) {
    if (available.tier != current.tier) {
        return available.tier > current.tier;
    }
    return available.tier == 1 && available.demZoom > current.demZoom;
}

/// The RGB texel a DEM decodes to 0 m, for its unpack vector (r * x + g * y + b * z - w):
/// (1, 134, 160) for Mapbox, (128, 0, 0) for Terrarium.
inline std::array<uint8_t, 3> seaLevelTexel(const std::array<float, 4>& unpack) {
    std::array<uint8_t, 3> texel{};
    double rest = unpack[3];
    for (std::size_t i = 0; i < 3; ++i) {
        if (unpack[i] <= 0.0f) {
            continue;
        }
        const double steps = i == 2 ? std::round(rest / unpack[i]) : std::floor(rest / unpack[i]);
        const double value = std::clamp(steps, 0.0, 255.0);
        texel[i] = static_cast<uint8_t>(value);
        rest -= value * unpack[i];
    }
    return texel;
}

/// How one draped layer group covers a drape target.
struct DrapeGroupFallback {
    int32_t layerIndex = 0;
    /// -1 = no usable tile, 0 = exact or deeper tiles, n > 0 = only an ancestor n zoom levels up
    int16_t deficit = -1;
};

/// Whether the content a drape target would render now (`now`) is strictly worse than what it
/// baked (`baked`): no layer group improved and at least one lost its content or fell back to a
/// coarser ancestor. The drape cache keeps its baked texture in that case, so a tile briefly
/// dropping out of the render set (eviction, reload) does not flash the drape empty.
///
/// Compared per layer group, not as a sum over groups: a sum lets one group's regression outweigh
/// another group's improvement, and the improvement - typically hillshade arriving from its real
/// DEM tile - is then discarded for as long as the target lives. Group sets that differ (style
/// change) are not comparable and never count as worse.
inline bool drapeFallbackStrictlyWorse(const std::vector<DrapeGroupFallback>& now,
                                       const std::vector<DrapeGroupFallback>& baked) {
    if (now.size() != baked.size()) {
        return false;
    }
    bool worse = false;
    for (std::size_t i = 0; i < now.size(); ++i) {
        const auto& n = now[i];
        const auto& b = baked[i];
        if (n.layerIndex != b.layerIndex) {
            return false;
        }
        if (n.deficit == b.deficit) {
            continue;
        }
        const bool improved = (b.deficit < 0) || (n.deficit >= 0 && n.deficit < b.deficit);
        if (improved) {
            return false;
        }
        worse = true;
    }
    return worse;
}

/// A drape target's size this frame; `current` is 0 for a new one, `side` is what it gets.
struct DrapeGrowth {
    uint32_t wanted = 0;
    uint32_t current = 0;
    double edge = 0.0;
    bool heldBack = false;
    uint32_t side = 0;
};

/// Draws `budget` texels of new or resized drapes a frame, nearest first; the rest keep their
/// texture or start at `smallest`, held back to grow later. True when any were held back.
inline bool rationDrapeGrowth(std::vector<DrapeGrowth>& drapes, std::size_t budget, uint32_t smallest) {
    std::vector<std::size_t> order(drapes.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::ranges::stable_sort(order, [&](std::size_t a, std::size_t b) { return drapes[a].edge > drapes[b].edge; });
    std::size_t left = budget;
    bool drewOne = false;
    bool heldAny = false;
    for (const auto i : order) {
        auto& drape = drapes[i];
        drape.side = drape.wanted;
        if (drape.wanted == drape.current) {
            drape.heldBack = false;
            continue;
        }
        const std::size_t cost = std::size_t{drape.wanted} * drape.wanted;
        // The nearest always goes, so held-back drapes keep growing frame by frame.
        if (budget == 0 || !drewOne || cost <= left) {
            left -= std::min(cost, left);
            drewOne = true;
            drape.heldBack = false;
            continue;
        }
        drape.side = drape.current ? drape.current : std::min(smallest, drape.wanted);
        drape.heldBack = true;
        heldAny = true;
        if (!drape.current) {
            left -= std::min(std::size_t{drape.side} * drape.side, left);
        }
    }
    return heldAny;
}

} // namespace terrain
} // namespace mln
