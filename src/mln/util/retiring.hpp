#pragma once

#include <algorithm>
#include <cstddef>
#include <deque>
#include <utility>

namespace mln {
namespace util {

/// What is let go of a few at a time, oldest first, so no one frame frees it all.
template <typename T>
class Retiring {
public:
    void add(T item) { items.push_back(std::move(item)); }

    /// Frees at most `count`, every one when `count` is 0; answers how many went.
    std::size_t release(std::size_t count) {
        const std::size_t freed = count == 0 ? items.size() : std::min(count, items.size());
        items.erase(items.begin(), items.begin() + static_cast<std::ptrdiff_t>(freed));
        return freed;
    }

    std::size_t size() const { return items.size(); }

private:
    std::deque<T> items;
};

} // namespace util
} // namespace mln
