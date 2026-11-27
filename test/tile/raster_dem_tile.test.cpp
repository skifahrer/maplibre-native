#include <mln/test/util.hpp>
#include <algorithm>
#include <chrono>
#include <thread>
#include <mln/test/fake_file_source.hpp>
#include <mln/tile/raster_dem_tile.hpp>
#include <mln/tile/raster_tile.hpp>
#include <mln/tile/tile_loader_impl.hpp>

#include <mln/style/style.hpp>
#include <mln/util/run_loop.hpp>
#include <mln/map/transform.hpp>
#include <mln/annotation/annotation_manager.hpp>
#include <mln/renderer/tile_parameters.hpp>
#include <mln/renderer/buckets/hillshade_bucket.hpp>
#include <mln/geometry/hillshade_slopes.hpp>
#include <mln/renderer/image_manager.hpp>
#include <mln/text/glyph_manager.hpp>
#include <mln/gfx/dynamic_texture_atlas.hpp>
#include <mln/util/tile_cover.hpp>

using namespace mln;

class RasterDEMTileTest {
public:
    util::SimpleIdentity uniqueID;
    std::shared_ptr<FileSource> fileSource = std::make_shared<FakeFileSource>();
    TransformState transformState;
    util::RunLoop loop;
    AnnotationManager annotationManager{style};
    std::shared_ptr<ImageManager> imageManager = ImageManager::create();
    std::shared_ptr<GlyphManager> glyphManager = std::make_shared<GlyphManager>();
    gfx::DynamicTextureAtlasPtr dynamicTextureAtlas;

    Tileset tileset{{"https://example.com"}, {0, 22}, "none"};
    TileParameters tileParameters;
    style::Style style;

    RasterDEMTileTest()
        : tileParameters{.pixelRatio = 1.0,
                         .debugOptions = MapDebugOptions(),
                         .transformState = transformState,
                         .fileSource = fileSource,
                         .mode = MapMode::Continuous,
                         .annotationManager = annotationManager.makeWeakPtr(),
                         .imageManager = imageManager,
                         .glyphManager = glyphManager,
                         .prefetchZoomDelta = 0,
                         .threadPool = {Scheduler::GetBackground(), uniqueID},
                         .dynamicTextureAtlas = dynamicTextureAtlas},
          style{fileSource, 1, tileParameters.threadPool} {}
};

// A file source with a cache, as the default one has: a tile asks it for a
// cache-only response first.
class CachingFakeFileSource : public FakeFileSource {
public:
    bool supportsCacheOnlyRequests() const override { return true; }
};

// Stands for the terrain: the tile only asks whether there is one.
class NoElevation : public util::TileElevationProvider {
public:
    std::optional<Range<double>> getTileElevationRange(const CanonicalTileID&) const override { return std::nullopt; }
};

// An elevation tile is requested high priority under terrain, and an
// imagery tile is not. A hillshade layer alone has nothing waiting for its
// elevation tile.
TEST(RasterDEMTile, RequestsAreHighPriorityUnderTerrain) {
    RasterDEMTileTest test;
    auto fileSource = std::static_pointer_cast<FakeFileSource>(test.fileSource);

    RasterDEMTile hillshade(OverscaledTileID(0, 0, 0), "hillshade", test.tileParameters, test.tileset);
    hillshade.setNecessity(TileNecessity::Required);
    ASSERT_EQ(fileSource->requests.size(), 1u);
    EXPECT_EQ(fileSource->requests.back()->resource.priority, Resource::Priority::Regular);

    NoElevation terrain;
    test.tileParameters.elevationProvider = &terrain;

    RasterDEMTile elevation(OverscaledTileID(0, 0, 0), "elevation", test.tileParameters, test.tileset);
    elevation.setNecessity(TileNecessity::Required);
    ASSERT_EQ(fileSource->requests.size(), 2u);
    EXPECT_EQ(fileSource->requests.back()->resource.priority, Resource::Priority::High);

    RasterTile imagery(OverscaledTileID(0, 0, 0), "imagery", test.tileParameters, test.tileset);
    imagery.setNecessity(TileNecessity::Required);
    ASSERT_EQ(fileSource->requests.size(), 3u);
    EXPECT_EQ(fileSource->requests.back()->resource.priority, Resource::Priority::Regular);
}

// Only an elevation tile with nothing to show is what the mesh waits for. Once
// the cache has answered with data, revalidating it is a regular request.
TEST(RasterDEMTile, RevalidationOfATileWithDataIsRegularPriority) {
    RasterDEMTileTest test;
    auto fileSource = std::make_shared<CachingFakeFileSource>();
    test.tileParameters.fileSource = fileSource;
    NoElevation terrain;
    test.tileParameters.elevationProvider = &terrain;

    {
        RasterDEMTile uncached(OverscaledTileID(0, 0, 0), "uncached", test.tileParameters, test.tileset);
        uncached.setNecessity(TileNecessity::Required);
        ASSERT_EQ(fileSource->requests.size(), 1u);
        ASSERT_EQ(fileSource->requests.back()->resource.loadingMethod, Resource::LoadingMethod::CacheOnly);

        Response miss;
        miss.noContent = true;
        miss.error = std::make_unique<Response::Error>(Response::Error::Reason::NotFound, "not cached");
        ASSERT_TRUE(fileSource->respond(Resource::Tile, miss));

        ASSERT_EQ(fileSource->requests.size(), 1u);
        EXPECT_EQ(fileSource->requests.back()->resource.loadingMethod, Resource::LoadingMethod::NetworkOnly);
        EXPECT_EQ(fileSource->requests.back()->resource.priority, Resource::Priority::High);

        // A failed answer leaves the tile with nothing, so asking again is High too.
        Response failure;
        failure.error = std::make_unique<Response::Error>(Response::Error::Reason::Connection, "offline");
        ASSERT_TRUE(fileSource->respond(Resource::Tile, failure));
        uncached.setNecessity(TileNecessity::Optional);
        uncached.setNecessity(TileNecessity::Required);
        ASSERT_EQ(fileSource->requests.size(), 1u);
        EXPECT_EQ(fileSource->requests.back()->resource.priority, Resource::Priority::High);
    }

    RasterDEMTile cached(OverscaledTileID(0, 0, 0), "cached", test.tileParameters, test.tileset);
    cached.setNecessity(TileNecessity::Required);
    ASSERT_EQ(fileSource->requests.size(), 1u);
    ASSERT_EQ(fileSource->requests.back()->resource.loadingMethod, Resource::LoadingMethod::CacheOnly);

    Response hit;
    hit.data = std::make_shared<std::string>("elevation");
    ASSERT_TRUE(fileSource->respond(Resource::Tile, hit));

    ASSERT_EQ(fileSource->requests.size(), 1u);
    EXPECT_EQ(fileSource->requests.back()->resource.loadingMethod, Resource::LoadingMethod::NetworkOnly);
    EXPECT_EQ(fileSource->requests.back()->resource.priority, Resource::Priority::Regular);
}

TEST(RasterDEMTile, setError) {
    RasterDEMTileTest test;
    RasterDEMTile tile(OverscaledTileID(0, 0, 0), "testSource", test.tileParameters, test.tileset);
    tile.setError(std::make_exception_ptr(std::runtime_error("test")));
    EXPECT_FALSE(tile.isRenderable());
    EXPECT_TRUE(tile.isLoaded());
    EXPECT_TRUE(tile.isComplete());
}

TEST(RasterDEMTile, onError) {
    RasterDEMTileTest test;
    RasterDEMTile tile(OverscaledTileID(0, 0, 0), "testSource", test.tileParameters, test.tileset);
    tile.onError(std::make_exception_ptr(std::runtime_error("test")), 0);
    EXPECT_FALSE(tile.isRenderable());
    EXPECT_TRUE(tile.isLoaded());
    EXPECT_TRUE(tile.isComplete());
}

TEST(RasterDEMTile, onParsed) {
    RasterDEMTileTest test;
    RasterDEMTile tile(OverscaledTileID(0, 0, 0), "testSource", test.tileParameters, test.tileset);
    tile.onParsed(std::make_unique<HillshadeBucket>(PremultipliedImage({16, 16}), Tileset::RasterEncoding::Mapbox), 0);
    EXPECT_TRUE(tile.isRenderable());
    EXPECT_TRUE(tile.isLoaded());
    EXPECT_TRUE(tile.isComplete());

    // Make sure that once we've had a renderable tile and then receive
    // erroneous data, we retain the previously rendered data and keep the tile
    // renderable.
    tile.setError(std::make_exception_ptr(std::runtime_error("Connection offline")));
    EXPECT_TRUE(tile.isRenderable());
    EXPECT_TRUE(tile.isLoaded());
    EXPECT_TRUE(tile.isComplete());

    // Then simulate a parsing failure and make sure that we keep it renderable
    // in this situation as well.
    tile.onError(std::make_exception_ptr(std::runtime_error("Parse error")), 0);
    ASSERT_TRUE(tile.isRenderable());
    EXPECT_TRUE(tile.isLoaded());
    EXPECT_TRUE(tile.isComplete());
}

TEST(RasterDEMTile, onParsedEmpty) {
    RasterDEMTileTest test;
    RasterDEMTile tile(OverscaledTileID(0, 0, 0), "testSource", test.tileParameters, test.tileset);
    tile.onParsed(nullptr, 0);
    EXPECT_FALSE(tile.isRenderable());
    EXPECT_TRUE(tile.isLoaded());
    EXPECT_TRUE(tile.isComplete());
}

namespace {
// Backfill only checks these exist; tests have no GPU context for real ones.
template <typename T>
std::shared_ptr<T> placeholder() {
    static char storage;
    return std::shared_ptr<T>(std::shared_ptr<T>{}, reinterpret_cast<T*>(&storage));
}
} // namespace

// 70 (for 68): after a backfill a prepared tile redraws its target, not rebuilds it.
TEST(RasterDEMTile, TerrainBackfillRedrawsThePreparedTarget) {
    RasterDEMTileTest test;
    RasterDEMTile tile(OverscaledTileID(1, 0, 0), "testSource", test.tileParameters, test.tileset);
    RasterDEMTile right(OverscaledTileID(1, 1, 0), "testSource", test.tileParameters, test.tileset);
    tile.onParsed(std::make_unique<HillshadeBucket>(PremultipliedImage({16, 16}), Tileset::RasterEncoding::Mapbox), 0);
    right.onParsed(std::make_unique<HillshadeBucket>(PremultipliedImage({16, 16}), Tileset::RasterEncoding::Mapbox), 0);

    auto* bucket = tile.getBucket();
    bucket->renderTarget = placeholder<RenderTarget>();
    bucket->prepareDEMTexture = placeholder<gfx::Texture2D>();
    bucket->renderTargetPrepared = true;

    tile.backfillBorder(right, DEMTileNeighbors::Right);

    EXPECT_TRUE(bucket->renderTargetPrepared);
    EXPECT_TRUE(bucket->borderChanged);
}

// 172 a DEM tile loaded again kept its old border mask: the new data's border was never filled
TEST(RasterDEMTile, NewDataTakesItsBorderAgain) {
    RasterDEMTileTest test;
    RasterDEMTile tile(OverscaledTileID(2, 1, 1), "testSource", test.tileParameters, test.tileset);
    RasterDEMTile right(OverscaledTileID(2, 2, 1), "testSource", test.tileParameters, test.tileset);
    tile.onParsed(std::make_unique<HillshadeBucket>(PremultipliedImage({16, 16}), Tileset::RasterEncoding::Mapbox), 0);
    right.onParsed(std::make_unique<HillshadeBucket>(PremultipliedImage({16, 16}), Tileset::RasterEncoding::Mapbox), 0);
    tile.backfillBorder(right, DEMTileNeighbors::Right);
    ASSERT_EQ(tile.neighboringTiles, DEMTileNeighbors::Right);

    tile.onParsed(std::make_unique<HillshadeBucket>(PremultipliedImage({16, 16}), Tileset::RasterEncoding::Mapbox), 0);
    EXPECT_EQ(tile.neighboringTiles, DEMTileNeighbors::Empty);
}

// 172 a top-row tile still has no neighbours above it after new data
TEST(RasterDEMTile, NewDataKeepsTheEdgesPastTheWorld) {
    RasterDEMTileTest test;
    RasterDEMTile tile(OverscaledTileID(1, 0, 0), "testSource", test.tileParameters, test.tileset);
    tile.onParsed(std::make_unique<HillshadeBucket>(PremultipliedImage({16, 16}), Tileset::RasterEncoding::Mapbox), 0);
    EXPECT_EQ(tile.neighboringTiles, DEMTileNeighbors::NoUpper);
}

// 70 (for 68): a tile with no target yet is still prepared from scratch.
TEST(RasterDEMTile, TerrainBackfillWithoutATargetPreparesAfresh) {
    RasterDEMTileTest test;
    RasterDEMTile tile(OverscaledTileID(1, 0, 0), "testSource", test.tileParameters, test.tileset);
    RasterDEMTile right(OverscaledTileID(1, 1, 0), "testSource", test.tileParameters, test.tileset);
    tile.onParsed(std::make_unique<HillshadeBucket>(PremultipliedImage({16, 16}), Tileset::RasterEncoding::Mapbox), 0);
    right.onParsed(std::make_unique<HillshadeBucket>(PremultipliedImage({16, 16}), Tileset::RasterEncoding::Mapbox), 0);
    tile.getBucket()->renderTargetPrepared = true;

    tile.backfillBorder(right, DEMTileNeighbors::Right);

    EXPECT_FALSE(tile.getBucket()->renderTargetPrepared);
    EXPECT_FALSE(tile.getBucket()->borderChanged);
}

namespace {
class ErrorRecorder : public TileObserver {
public:
    std::string error;
    void onTileError(Tile&, std::exception_ptr err) override {
        try {
            std::rethrow_exception(err);
        } catch (const std::exception& e) {
            error = e.what();
        }
    }
};
} // namespace

// 196 the worker makes the slopes with the bucket, so a new tile costs the frame no prepare pass
TEST(RasterDEMTile, TheWorkerMakesTheSlopes) {
    RasterDEMTileTest test;
    auto fileSource = std::static_pointer_cast<FakeFileSource>(test.fileSource);
    RasterDEMTile tile(OverscaledTileID(3, 2, 1), "testSource", test.tileParameters, test.tileset);
    ErrorRecorder errors;
    tile.setObserver(&errors);
    tile.setNecessity(TileNecessity::Required);
    Response response;
    response.data = std::make_shared<std::string>(encodePNG(PremultipliedImage({16, 16})));
    ASSERT_TRUE(fileSource->respond(Resource::Tile, response));
    // the parse runs on a worker thread and answers through the loop, which runOnce does not wait for
    const auto giveUp = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!tile.getBucket() && errors.error.empty() && std::chrono::steady_clock::now() < giveUp) {
        test.loop.runOnce();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_EQ(errors.error, "");
    ASSERT_NE(tile.getBucket(), nullptr);
    ASSERT_TRUE(tile.getBucket()->slopes);
    EXPECT_EQ(tile.getBucket()->slopes->size, Size(18, 18));
}

// 196 a backfill remakes the slopes' edges from the new border and marks them for upload
TEST(RasterDEMTile, BackfillRemakesTheSlopesEdges) {
    RasterDEMTileTest test;
    RasterDEMTile tile(OverscaledTileID(1, 0, 0), "testSource", test.tileParameters, test.tileset);
    RasterDEMTile right(OverscaledTileID(1, 1, 0), "testSource", test.tileParameters, test.tileset);
    PremultipliedImage steep({16, 16});
    for (size_t i = 0; i < steep.bytes(); i += 4) {
        steep.data[i] = 1;
        steep.data[i + 1] = static_cast<uint8_t>((i / 4) % 16 * 15);
        steep.data[i + 3] = 255;
    }
    tile.onParsed(std::make_unique<HillshadeBucket>(PremultipliedImage({16, 16}), Tileset::RasterEncoding::Mapbox), 0);
    right.onParsed(std::make_unique<HillshadeBucket>(std::move(steep), Tileset::RasterEncoding::Mapbox), 0);
    auto* bucket = tile.getBucket();
    bucket->makeSlopes(1);
    const auto before = bucket->slopes->clone();

    tile.backfillBorder(right, DEMTileNeighbors::Right);

    EXPECT_TRUE(bucket->slopesChanged);
    const auto fresh = HillshadeSlopes::make(bucket->getDEMData(), 1);
    ASSERT_EQ(bucket->slopes->bytes(), fresh.bytes());
    EXPECT_TRUE(std::equal(fresh.data.get(), fresh.data.get() + fresh.bytes(), bucket->slopes->data.get()));
    EXPECT_FALSE(std::equal(before.data.get(), before.data.get() + before.bytes(), fresh.data.get()));
}

// 196 a bucket made without slopes keeps the prepare pass and gets none on a backfill
TEST(RasterDEMTile, BackfillWithoutSlopesLeavesThemOut) {
    RasterDEMTileTest test;
    RasterDEMTile tile(OverscaledTileID(1, 0, 0), "testSource", test.tileParameters, test.tileset);
    RasterDEMTile right(OverscaledTileID(1, 1, 0), "testSource", test.tileParameters, test.tileset);
    tile.onParsed(std::make_unique<HillshadeBucket>(PremultipliedImage({16, 16}), Tileset::RasterEncoding::Mapbox), 0);
    right.onParsed(std::make_unique<HillshadeBucket>(PremultipliedImage({16, 16}), Tileset::RasterEncoding::Mapbox), 0);

    tile.backfillBorder(right, DEMTileNeighbors::Right);

    EXPECT_FALSE(tile.getBucket()->slopes);
    EXPECT_FALSE(tile.getBucket()->slopesChanged);
}
