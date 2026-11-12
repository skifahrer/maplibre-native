#pragma once

#include <mln/map/transform_state.hpp>

namespace mln {

/// Whether symbols from newly loaded tiles are placed within 30 ms rather than at the usual pace.
inline bool placesNewSymbolsSoon(bool symbolBucketsAdded, const TransformState& state) {
    // While the camera moves, tiles add buckets nearly every frame, and placing that
    // often re-placed every symbol layer each frame; new labels come at the usual pace.
    return symbolBucketsAdded && state.getPitch() == 0.0 && !state.isChanging();
}

} // namespace mln
