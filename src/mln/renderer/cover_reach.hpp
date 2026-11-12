#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

namespace mln {

/// How far the terrain's cover reaches, eased so the fog drawn in front of its cut holds still.
class CoverReach {
public:
    /// Takes this frame's distance to the first dropped tile, infinite when none was dropped,
    /// and the fog's end, past which the reach makes no difference.
    double update(double reach, double seconds, double fogEnd) {
        const double far = fogEnd > 0.0 ? fogEnd : farEnough;
        const bool first = std::isnan(lastSeconds);
        const double dt = first ? 0.0 : std::max(0.0, seconds - lastSeconds);
        lastSeconds = seconds;
        if (!std::isfinite(metres) && std::isfinite(reach)) {
            metres = first ? reach : std::max(reach, far);
        }
        if (reach <= metres) {
            // In quickly, so little past the cut shows, but not in one frame
            metres = reach + (metres - reach) * std::exp(-dt / inTime);
            fartherSince = std::numeric_limits<double>::quiet_NaN();
            return metres;
        }
        // The tile set flickers at the cap; only a reach that stays farther lets the fog out
        if (std::isnan(fartherSince)) {
            fartherSince = seconds;
        }
        if (seconds - fartherSince >= hold) {
            metres = std::isfinite(reach) ? reach + (metres - reach) * std::exp(-dt / outTime)
                                          : metres * std::exp(dt / outTime);
            if (metres >= far && !std::isfinite(reach)) {
                metres = reach;
            }
        }
        return metres;
    }

    double value() const { return metres; }

    static constexpr double inTime = 0.12;
    static constexpr double hold = 1.0;
    static constexpr double outTime = 0.6;
    static constexpr double farEnough = 1.0e7;

private:
    double metres = std::numeric_limits<double>::infinity();
    double lastSeconds = std::numeric_limits<double>::quiet_NaN();
    double fartherSince = std::numeric_limits<double>::quiet_NaN();
};

} // namespace mln
