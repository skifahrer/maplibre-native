#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <utility>

namespace mln {
namespace util {

/// A value set once, possibly on another thread, that readers can poll or wait for.
template <typename T>
class Pending {
public:
    /// Returns false when a value was set already; the first one stays.
    bool set(T newValue) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (value) {
                return false;
            }
            value.emplace(std::move(newValue));
        }
        changed.notify_all();
        return true;
    }

    bool isSet() const {
        std::lock_guard<std::mutex> lock(mutex);
        return value.has_value();
    }

    const T& wait() const {
        std::unique_lock<std::mutex> lock(mutex);
        changed.wait(lock, [this] { return value.has_value(); });
        return *value;
    }

private:
    mutable std::mutex mutex;
    mutable std::condition_variable changed;
    std::optional<T> value;
};

} // namespace util
} // namespace mln
