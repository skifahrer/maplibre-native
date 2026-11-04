#include <mln/test/util.hpp>
#include <mln/test/fake_file_source.hpp>
#include <mln/tile/raster_dem_tile.hpp>
#include <mln/tile/tile_loader_impl.hpp>

#include <mln/style/style.hpp>
#include <mln/util/run_loop.hpp>
#include <mln/map/transform.hpp>
#include <mln/annotation/annotation_manager.hpp>
#include <mln/renderer/tile_parameters.hpp>
#include <mln/renderer/buckets/hillshade_bucket.hpp>
#include <mln/renderer/image_manager.hpp>
#include <mln/text/glyph_manager.hpp>
#include <mln/gfx/dynamic_texture_atlas.hpp>

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
