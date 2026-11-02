#include <mln/renderer/dem_elevation_provider.hpp>

#include <mln/renderer/sources/render_raster_dem_source.hpp>

#include <algorithm>

namespace mln {

DEMElevationProvider::DEMElevationProvider(const RenderSource* demSource_,
                                           double exaggeration_,
                                           std::optional<float> plane_)
    : demSource(demSource_ && demSource_->baseImpl->type == style::SourceType::RasterDEM
                    ? static_cast<const RenderRasterDEMSource*>(demSource_)
                    : nullptr),
      exaggeration(exaggeration_),
      plane(plane_) {}

std::optional<Range<double>> DEMElevationProvider::getTileElevationRange(const CanonicalTileID& id) const {
    if (!demSource) {
        return std::nullopt;
    }
    if (const auto known = answers.find(id); known != answers.end()) {
        return known->second;
    }
    return answers.emplace(id, lookUp(id)).first->second;
}

std::optional<Range<double>> DEMElevationProvider::lookUp(const CanonicalTileID& id) const {

    // The tile's own DEM, or failing that the deepest loaded ancestor's, or failing that the
    // range of the loaded DEM nearest the tile (see the header).
    auto range = demSource->getElevationRange(id);
    if (!range) {
        range = demSource->getNearbyElevationRange(id);
    }
    if (!range) {
        return std::nullopt;
    }

    // A tile with no ground in it reaches down to sea level, its clay face and the table
    if (plane && range->min <= *plane + 1) {
        range->min = std::min(range->min, 0);
    }

    // Exaggeration is applied to the mesh in the terrain vertex shader, so the bounds
    // have to carry it too, or an exaggerated peak would still be culled.
    return Range<double>{range->min * exaggeration, range->max * exaggeration};
}

} // namespace mln
