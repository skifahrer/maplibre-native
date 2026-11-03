#include <mln/test/util.hpp>
#include <mln/test/stub_file_source.hpp>
#include <mln/test/map_adapter.hpp>

#include <mln/gfx/headless_frontend.hpp>
#include <mln/map/bound_options.hpp>
#include <mln/map/camera.hpp>
#include <mln/map/map_options.hpp>
#include <mln/map/mode.hpp>
#include <mln/renderer/render_terrain.hpp>
#include <mln/style/layer.hpp>
#include <mln/style/style.hpp>
#include <mln/style/types.hpp>
#include <mln/util/image.hpp>
#include <mln/util/run_loop.hpp>
#include <mln/util/tile_cover.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>
#include <numbers>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace mln;

namespace {

std::string makeFlatDEMTile() {
    constexpr uint32_t size = 64;
    PremultipliedImage image({size, size});
    for (uint32_t i = 0; i < size * size; ++i) {
        uint8_t* px = image.data.get() + i * 4;
        px[0] = 1;
        px[1] = 173;
        px[2] = 176;
        px[3] = 255;
    }
    return encodePNG(image);
}

// The flat DEM tile above decodes to this height everywhere.
constexpr double flatDEMMeters = 1000.0;

// A vector source whose minzoom is deeper than the far field needs, beside the DEM. The
// vector layer starts hidden, so the DEM can load before the source asks for anything.
std::string deepSourceStyle(int demMinZoom, int demMaxZoom) {
    return R"STYLE({
  "version": 8,
  "sources": {
    "dem": {
      "type": "raster-dem",
      "tiles": ["http://example.com/dem/{z}-{x}-{y}.png"],
      "encoding": "mapbox",
      "minzoom": )STYLE" +
           std::to_string(demMinZoom) + R"STYLE(,
      "maxzoom": )STYLE" +
           std::to_string(demMaxZoom) + R"STYLE(,
      "tileSize": 64
    },
    "parcels": {
      "type": "vector",
      "tiles": ["http://example.com/parcels/{z}-{x}-{y}.pbf"],
      "minzoom": 13,
      "maxzoom": 14
    }
  },
  "terrain": {"source": "dem", "exaggeration": 1.0},
  "layers": [{
    "id": "parcels",
    "type": "fill",
    "source": "parcels",
    "source-layer": "parcels",
    "layout": {"visibility": "none"}
  }]
})STYLE";
}

CanonicalTileID tileIDFromURL(const std::string& url) {
    const size_t slash = url.rfind('/');
    unsigned z = 0, x = 0, y = 0;
    std::sscanf(url.c_str() + slash + 1, "%u-%u-%u", &z, &x, &y);
    return CanonicalTileID(static_cast<uint8_t>(z), x, y);
}

// The terrain's height everywhere, for the tile cover the renderer draws from the DEM it loaded.
class FlatTerrain : public util::TileElevationProvider {
public:
    std::optional<Range<double>> getTileElevationRange(const CanonicalTileID&) const override {
        return Range<double>{flatDEMMeters, flatDEMMeters};
    }
};

// A steep view over terrain, with a vector source and the DEM both behind a stub file source that
// records what each one is asked for.
class TerrainSourceCoverTest {
public:
    struct Options {
        int demMinZoom = 0;
        int demMaxZoom = 6;
        TerrainLoadMode mode = TerrainLoadMode::Quality;
        double pitch = 70.0;
        double zoom = 13.0;
        Size size{1200, 800};
    };

    explicit TerrainSourceCoverTest(const Options& options)
        : zoom(options.zoom),
          fileSource(std::make_shared<StubFileSource>(ResourceOptions::Default(), ClientOptions())),
          demTile(makeFlatDEMTile()),
          frontend(options.size, 1),
          map(frontend,
              MapObserver::nullObserver(),
              fileSource,
              MapOptions().withMapMode(MapMode::Static).withSize(frontend.getSize())) {
        fileSource->tileResponse = [this](const Resource& resource) {
            Response res;
            if (resource.url.find("/parcels/") != std::string::npos) {
                parcelTiles.insert(tileIDFromURL(resource.url));
                res.noContent = true;
            } else {
                demTiles.insert(tileIDFromURL(resource.url));
                res.data = std::make_shared<std::string>(demTile);
            }
            return res;
        };
        map.setTerrainLoadMode(options.mode);
        map.getStyle().loadJSON(deepSourceStyle(options.demMinZoom, options.demMaxZoom));
        map.setBounds(BoundOptions().withMaxPitch(85.0));
        map.jumpTo(CameraOptions().withCenter(LatLng{38.26, -120.68}).withZoom(options.zoom).withPitch(options.pitch));
    }

    void render(int frames) {
        for (int i = 0; i < frames; ++i) {
            loop.runOnce();
            frontend.render(map);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    // Renders until the DEM has stopped arriving: the elevated cover is then the same from one
    // frame to the next.
    void settleDEM() {
        size_t seen = 0;
        int quiet = 0;
        for (int frames = 0; frames < 200 && quiet < 10; ++frames) {
            render(1);
            quiet = demTiles.size() == seen ? quiet + 1 : 0;
            seen = demTiles.size();
        }
        ASSERT_GE(quiet, 10) << "the DEM was still loading after 200 frames";
    }

    void showParcels() { map.getStyle().getLayer("parcels")->setVisibility(style::VisibilityType::Visible); }

    size_t sourceTileLimit() const {
        return terrainLoadBudget(map.getTerrainLoadMode()).maxMeshTiles * RenderTerrain::sourceTilesPerMeshTile;
    }

    // Where the camera stands on the ground, from the free-camera API, worked out independently of
    // RenderTerrain.
    std::array<double, 2> cameraGround() const {
        const vec3 position = *map.getFreeCameraOptions().position;
        return {{position[0], position[1]}};
    }

    // The `count` tiles of the parcels source's whole cover nearest `origin` (normalized
    // web-mercator), before any cap: tileCover over the flat plateau the DEM tiles describe, or
    // over flat ground without terrain. `coverSize` gets the size of the whole cover.
    std::set<CanonicalTileID> nearestCover(const std::array<double, 2>& origin,
                                           size_t count,
                                           size_t& coverSize,
                                           bool overTerrain = true) const {
        const auto coverZoom = static_cast<uint8_t>(std::clamp(static_cast<int>(zoom), 13, 14));
        FlatTerrain terrain;
        const util::TileCoverParameters params{.transformState = map.getTransformState(),
                                               .elevationProvider = overTerrain ? &terrain : nullptr};
        std::vector<std::pair<double, CanonicalTileID>> ranked;
        for (const OverscaledTileID& id : util::tileCover(params, coverZoom, Range<uint8_t>{13, 14}, coverZoom)) {
            const double scale = static_cast<double>(1u << id.canonical.z);
            const double dx = (id.canonical.x + 0.5) / scale + id.wrap - origin[0];
            const double dy = (id.canonical.y + 0.5) / scale - origin[1];
            ranked.emplace_back(dx * dx + dy * dy, id.canonical);
        }
        std::sort(ranked.begin(), ranked.end());
        coverSize = ranked.size();
        std::set<CanonicalTileID> nearest;
        for (size_t i = 0; i < std::min(count, ranked.size()); ++i) nearest.insert(ranked[i].second);
        return nearest;
    }

    // The map centre in normalized web-mercator.
    std::array<double, 2> mapCentre() const {
        const LatLng centre = *map.getCameraOptions({}).center;
        const double latRad = centre.latitude() * std::numbers::pi / 180.0;
        return {{centre.longitude() / 360.0 + 0.5,
                 0.5 - std::log(std::tan(std::numbers::pi / 4.0 + latRad / 2.0)) / (2.0 * std::numbers::pi)}};
    }

    double zoom;
    util::RunLoop loop;
    std::shared_ptr<StubFileSource> fileSource;
    std::string demTile;
    HeadlessFrontend frontend;
    MapAdapter map;
    std::set<CanonicalTileID> parcelTiles;
    std::set<CanonicalTileID> demTiles;
};

// The source asks for the tiles nearest the camera and no others: the same set as the nearest
// of the whole cover, measured from where the camera stands on the ground.
void expectOnlyTheTilesNearestTheCamera(const TerrainSourceCoverTest::Options& options) {
    TerrainSourceCoverTest test(options);
    // The DEM arrives first: the elevated cover shifts as it lands, and a source that asked for
    // the tiles on each side of that shift has asked for more than its cap in all.
    test.settleDEM();
    test.showParcels();
    test.render(10);

    const size_t limit = test.sourceTileLimit();
    size_t coverSize = 0;
    const std::set<CanonicalTileID> nearestTheCamera = test.nearestCover(test.cameraGround(), limit, coverSize);
    ASSERT_GT(coverSize, limit) << "the view has to cover more than the cap";

    // A cap measured from the map centre would keep a few other tiles here.
    size_t unused = 0;
    ASSERT_NE(nearestTheCamera, test.nearestCover(test.mapCentre(), limit, unused));

    EXPECT_EQ(test.parcelTiles.size(), limit);
    EXPECT_EQ(test.parcelTiles, nearestTheCamera);
}

} // namespace

// A source whose minzoom is deeper than the far field is covered at that minzoom out to the
// horizon, thousands of tiles at a steep pitch. Under terrain it asks for the nearest few per
// tile of the mesh-tile cap, nearest the camera, not the map centre.
TEST(TerrainSourceCover, ASourceRequestsOnlyTheTilesNearestTheCamera) {
    expectOnlyTheTilesNearestTheCamera({.mode = TerrainLoadMode::Quality, .size = {1600, 1600}});
}

TEST(TerrainSourceCover, TheCapFollowsTheTerrainLoadMode) {
    expectOnlyTheTilesNearestTheCamera({.mode = TerrainLoadMode::Performance});
}

// The DEM loads the tiles the mesh is drawn on, far ones merged to fit the mesh-tile cap, not a
// cover of its own: a mesh tile merged to a coarser level needs the DEM tile at that level, and
// the DEM's own cover here holds far more tiles than the cap. Past maxzoom, its ancestor.
TEST(TerrainSourceCover, TheDEMSourceLoadsTheMeshCover) {
    TerrainSourceCoverTest test({.demMinZoom = 0, .demMaxZoom = 13, .mode = TerrainLoadMode::Performance});
    test.settleDEM();
    const size_t cap = terrainLoadBudget(TerrainLoadMode::Performance).maxMeshTiles;
    const auto cover = util::terrainMeshCover();
    ASSERT_FALSE(cover.empty());
    EXPECT_LE(cover.size(), cap);

    test.demTiles.clear();
    test.map.jumpTo(CameraOptions().withBearing(40.0));
    test.settleDEM();
    const auto turned = util::terrainMeshCover();
    for (const auto& id : test.demTiles) {
        EXPECT_TRUE(std::any_of(turned.begin(), turned.end(), [&](const UnwrappedTileID& tile) {
            return tile.canonical.scaledTo(std::min<uint8_t>(tile.canonical.z, 13)) == id;
        })) << int(id.z) << "/" << id.x << "/" << id.y;
    }
}

// Without terrain there is no mesh to size the cap from, and the source keeps its whole cover.
TEST(TerrainSourceCover, ASourceKeepsItsWholeCoverWithoutTerrain) {
    TerrainSourceCoverTest test({.mode = TerrainLoadMode::Performance});
    test.settleDEM();
    test.map.getStyle().setTerrain(nullptr);
    test.showParcels();
    test.render(10);

    size_t coverSize = 0;
    test.nearestCover(test.cameraGround(), 0, coverSize, false);
    ASSERT_GT(coverSize, test.sourceTileLimit()) << "the view has to cover more than the cap would allow";
    EXPECT_EQ(test.parcelTiles.size(), coverSize);
}

namespace {

// The finest mesh tile and the one nearest the ground under the camera, which is off screen.
struct MeshDetail {
    uint8_t finest = 0;
    uint8_t nearestTheCamera = 0;
};

MeshDetail meshDetail(const std::array<double, 2>& ground) {
    MeshDetail detail;
    double nearest = std::numeric_limits<double>::max();
    for (const UnwrappedTileID& tile : util::terrainMeshCover()) {
        detail.finest = std::max(detail.finest, tile.canonical.z);
        const double scale = static_cast<double>(1u << tile.canonical.z);
        const double dx = (tile.canonical.x + 0.5) / scale + tile.wrap - ground[0];
        const double dy = (tile.canonical.y + 0.5) / scale - ground[1];
        if (dx * dx + dy * dy < nearest) {
            nearest = dx * dx + dy * dy;
            detail.nearestTheCamera = tile.canonical.z;
        }
    }
    return detail;
}

} // namespace

// Once loaded, a camera that stands still draws the same mesh and asks for nothing new: a frame
// that swapped in other tiles is the 3D shading blinking with nothing moving.
TEST(TerrainSourceCover, AStillCameraKeepsItsMeshAndItsDEM) {
    TerrainSourceCoverTest test({.demMinZoom = 0, .demMaxZoom = 13, .mode = TerrainLoadMode::Balanced});
    test.settleDEM();
    const auto settled = util::terrainMeshCover();
    ASSERT_FALSE(settled.empty());
    test.demTiles.clear();
    for (int frame = 0; frame < 20; ++frame) {
        test.render(1);
        ASSERT_EQ(util::terrainMeshCover(), settled) << "frame " << frame;
    }
    EXPECT_TRUE(test.demTiles.empty()) << test.demTiles.size() << " DEM tiles asked for again";
}

// A short drag keeps the ground under the camera at the finest detail in every frame, not just
// once the DEM has settled again: one coarse frame is the blink the 2658 build fixed.
TEST(TerrainSourceCover, ASmallPanKeepsTheGroundNearestTheCameraFineInEveryFrame) {
    TerrainSourceCoverTest test({.demMinZoom = 0, .demMaxZoom = 13, .mode = TerrainLoadMode::Balanced, .pitch = 75.0});
    test.settleDEM();
    const MeshDetail before = meshDetail(test.cameraGround());
    ASSERT_EQ(before.nearestTheCamera, before.finest) << "the settled mesh is coarse nearest the camera";

    const LatLng centre = *test.map.getCameraOptions({}).center;
    for (int step = 1; step <= 10; ++step) {
        test.map.jumpTo(
            CameraOptions().withCenter(LatLng{centre.latitude() + step * 0.0004, centre.longitude() - step * 0.0002}));
        test.render(1);
        const MeshDetail now = meshDetail(test.cameraGround());
        EXPECT_EQ(now.nearestTheCamera, before.finest) << "step " << step << ": the ground nearest the camera went coarse";
    }
    test.settleDEM();
    EXPECT_EQ(meshDetail(test.cameraGround()).nearestTheCamera, before.finest);
}

