#pragma once

#include <mln/style/terrain.hpp>
#include <mln/util/immutable.hpp>
#include <optional>
#include <string>

namespace mln {
namespace style {

class Terrain::Impl {
public:
    Impl()
        : exaggeration(1.0f) {}
    Impl(const std::string& sourceID_, float exaggeration_)
        : sourceID(sourceID_),
          exaggeration(exaggeration_) {}

    std::string sourceID;
    float exaggeration;
    /// The height the DEM writes where it has no ground; nil when it writes none.
    std::optional<float> plane;

    bool operator==(const Impl& other) const {
        return sourceID == other.sourceID && exaggeration == other.exaggeration && plane == other.plane;
    }
};

} // namespace style
} // namespace mln
