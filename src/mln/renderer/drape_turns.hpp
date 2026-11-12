#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <numeric>
#include <vector>

namespace mln {

/// The order in which drape targets may use a frame's re-render budget, longest held first.
inline std::vector<std::size_t> drapeTurns(const std::vector<uint32_t>& framesHeld) {
    std::vector<std::size_t> order(framesHeld.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    // A fixed order spent the budget on the same targets every frame while content kept moving.
    std::ranges::stable_sort(order, std::greater<>{}, [&](std::size_t i) { return framesHeld[i]; });
    return order;
}

} // namespace mln
