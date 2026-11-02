#pragma once

#include <mln/map/transform_state.hpp>
#include <mln/tile/tile_id.hpp>
#include <mln/style/types.hpp>
#include <mln/util/geometry.hpp>
#include <mln/util/range.hpp>

#include <array>
#include <map>
#include <vector>
#include <memory>
#include <numbers>
#include <optional>
#include <set>
#include <tuple>

namespace mln {

class LatLngBounds;

namespace util {

// Helper class to stream tile-cover results per row
class TileCover {
public:
    TileCover(const LatLngBounds&, uint8_t z);
    // When project == true, projects the geometry points to tile coordinates
    TileCover(const Geometry<double>&, uint8_t z, bool project = true);
    ~TileCover();

    std::optional<UnwrappedTileID> next();
    bool hasNext();

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};

/// Supplies the elevation range of a tile, so that the tile cover can test terrain
/// against the view frustum instead of the flat ground plane. Terrain rising towards
/// the camera occupies screen space that its flat footprint does not, so without this
/// a tile whose relief is in view is judged off-screen and never requested - which
/// leaves the terrain mesh with nothing to drape. Implemented over the DEM source's
/// loaded tiles; see maplibre-gl-js CoveringTilesOptions::terrain, which this mirrors.
class TileElevationProvider {
public:
    virtual ~TileElevationProvider() = default;

    /// The lowest and highest elevation within the tile, in meters, or nullopt when
    /// no DEM covering the tile is loaded (the caller then falls back to flat).
    virtual std::optional<Range<double>> getTileElevationRange(const CanonicalTileID&) const = 0;
};

struct TileCoverParameters {
    TransformState transformState;
    double tileLodMinRadius = 3;
    double tileLodScale = 1;
    double tileLodPitchThreshold = (60.0 / 180.0) * std::numbers::pi;
    TileLodMode tileLodMode = TileLodMode::Default;
    /// Round the per-tile zoom rather than flooring it, as `coveringZoomLevel` does for the
    /// nominal zoom. GL JS carries the same distinction as `roundZoom`, set by raster and
    /// video sources. Only `TileLodMode::Adaptive` chooses a zoom per tile, so only it reads
    /// this.
    bool roundZoom = false;
    /// Cover no more than this many tiles: the ones whose centers lie nearest `tileLimitOrigin`,
    /// which come back in that order. The cover is walked nearest first and stops at the limit,
    /// so its cost follows the limit and not the size of the view: a source whose minzoom is
    /// deeper than the far field's zoom would otherwise enumerate (and sort) every tile out to the
    /// horizon at a steep pitch, a couple of hundred thousand of them, to keep a few hundred.
    /// 0 means no limit.
    size_t tileLimit = 0;
    /// Where `tileLimit` measures from (normalized web-mercator).
    std::array<double, 2> tileLimitOrigin{{0.0, 0.0}};
    /// Adaptive only: levels finer than the view's zoom the ground near the camera may go
    /// (nearDetailLevels()). The DEM source and the terrain mesh keep 0: the mesh near the camera
    /// is already finer than the view, and it is the draped sources that were stretched over it.
    uint8_t nearDetail = 0;
    /// Optional; when null the cover is computed against the flat ground plane, as
    /// it was before terrain support.
    const TileElevationProvider* elevationProvider = nullptr;
    /// Normalized web-mercator point the default (non-adaptive) level of detail measures its
    /// distance from, in place of the map centre: tiles within `tileLodMinRadius` of it get the
    /// finest zoom, and the zoom drops with distance from it. Unset (the default) measures from
    /// the map centre. Only read once the pitch passes `tileLodPitchThreshold`.
    std::optional<std::array<double, 2>> tileLodOrigin = std::nullopt;
};

int32_t coveringZoomLevel(double z, style::SourceType type, uint16_t tileSize) noexcept;

/// The levels an adaptive cover's near field may go past the view's zoom: the slope just ahead of
/// a tilted camera is magnified on screen, and drawn from tiles at the view's zoom it blurs.
uint8_t nearDetailLevels() noexcept;

/// The terrain mesh's last cover, which its DEM source loads instead of a cover of its own: a mesh
/// tile merged to fit the tile budget needs the DEM tile at its own level. Empty without terrain.
std::vector<UnwrappedTileID> terrainMeshCover();
void setTerrainMeshCover(std::vector<UnwrappedTileID> cover);

std::vector<OverscaledTileID> tileCover(const TileCoverParameters& state,
                                        uint8_t z,
                                        const Range<uint8_t> zoomRange,
                                        const std::optional<uint8_t>& overscaledZ = std::nullopt);

/// One frame's covers, shared by sources asking for the same one.
/// The key is every input but the frame's camera and detail.
class TileCoverCache {
public:
    std::vector<OverscaledTileID> cover(const TileCoverParameters& state,
                                        uint8_t z,
                                        Range<uint8_t> zoomRange,
                                        const std::optional<uint8_t>& overscaledZ = std::nullopt);

private:
    using Key = std::tuple<uint8_t,
                           uint8_t,
                           uint8_t,
                           int16_t,
                           bool,
                           uint8_t,
                           size_t,
                           std::array<double, 2>,
                           const TileElevationProvider*,
                           std::optional<std::array<double, 2>>>;
    std::map<Key, std::vector<OverscaledTileID>> covers;
};

/// Keep only the tiles whose (elevation-aware) bounds intersect the view frustum.
/// The terrain mesh is built by subdividing the DEM source's render tiles, which
/// include large low-zoom ancestors when the DEM is sparse; most of such a tile's
/// area is off-screen, and meshing it creates drape targets no on-screen source
/// covers - so they render empty. Culling the mesh set to the frustum drops that
/// off-screen area. Uses params.elevationProvider the same way tileCover does.
std::set<UnwrappedTileID> frustumCull(const TileCoverParameters& state, const std::set<UnwrappedTileID>& tiles);

/// Keep the `maxTiles` tiles whose centres lie nearest `origin` (normalized web-mercator; pass
/// the ground under the camera to keep those nearest the camera), nearest first; all of them
/// when there are no more than that or `maxTiles` is 0. The kept tiles are in this order, not
/// the order tileCover returned them in. Ties go by tile ID, so the same view keeps the same
/// tiles from one call to the next.
void keepNearestTiles(std::vector<OverscaledTileID>& tiles, size_t maxTiles, const std::array<double, 2>& origin);

/// Squared distance from `origin` (normalized web-mercator) to the nearest point of the tile.
double nearestPointDistance2(const CanonicalTileID& id, int16_t wrap, const std::array<double, 2>& origin);

/// Merge the tiles farthest from `origin` into their parents until no more than `maxTiles` are
/// left: the far ground goes coarser and the cover stays whole out to the horizon, where
/// keepNearestTiles cuts it short. The nearest keep their level: no parent is made wider than
/// twice its distance from `origin`, so the ground near it is never coarsened.
/// Parents stop at `minZoom`; where that is still too many, the nearest are kept. Ties go by tile
/// ID, as in keepNearestTiles.
void mergeFarthestTiles(std::vector<OverscaledTileID>& tiles,
                        size_t maxTiles,
                        const std::array<double, 2>& origin,
                        uint8_t minZoom);
std::vector<UnwrappedTileID> tileCover(const LatLngBounds&, uint8_t z);
std::vector<UnwrappedTileID> tileCover(const Geometry<double>&, uint8_t z);

// Compute only the count of tiles needed for tileCover
uint64_t tileCount(const LatLngBounds&, uint8_t z) noexcept;
uint64_t tileCount(const Geometry<double>&, uint8_t z);

} // namespace util
} // namespace mln
