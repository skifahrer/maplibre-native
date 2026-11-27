#include <mln/test/util.hpp>

#include <mln/geometry/dem_data.hpp>
#include <mln/geometry/hillshade_slopes.hpp>
#include <mln/util/image.hpp>
#include <mln/util/tileset.hpp>

#include <functional>

using namespace mln;

namespace {

PremultipliedImage terrarium(uint32_t side, const std::function<double(uint32_t, uint32_t)>& metres) {
    PremultipliedImage img({side, side});
    for (uint32_t y = 0; y < side; y++) {
        for (uint32_t x = 0; x < side; x++) {
            const double v = metres(x, y) + 32768.0;
            uint8_t* pixel = img.data.get() + (y * side + x) * 4;
            pixel[0] = static_cast<uint8_t>(static_cast<int>(v) / 256);
            pixel[1] = static_cast<uint8_t>(static_cast<int>(v) % 256);
            pixel[2] = static_cast<uint8_t>((v - static_cast<int>(v)) * 256);
            pixel[3] = 255;
        }
    }
    return img;
}

std::array<uint8_t, 4> at(const PremultipliedImage& img, uint32_t x, uint32_t y) {
    const uint8_t* p = img.data.get() + (y * img.size.width + x) * 4;
    return {p[0], p[1], p[2], p[3]};
}

bool sameEverywhere(const PremultipliedImage& img, std::array<uint8_t, 4> expected) {
    for (uint32_t y = 0; y < img.size.height; y++) {
        for (uint32_t x = 0; x < img.size.width; x++) {
            if (at(img, x, y) != expected) return false;
        }
    }
    return true;
}

} // namespace

// 196 the tile and a one-pixel ring, as the prepare pass's target
TEST(HillshadeSlopes, CoversTheTileAndItsRing) {
    const DEMData dem(terrarium(16, [](auto, auto) { return 700.0; }), Tileset::RasterEncoding::Terrarium);
    const auto slopes = HillshadeSlopes::make(dem, 12);
    EXPECT_EQ(slopes.size, Size(18, 18));
    EXPECT_TRUE(sameEverywhere(slopes, {128, 128, 255, 255})) << "level ground has no slope";
}

// 196 the prepare shader's numbers: Sobel sums scaled to world slope, east in red, south in green
TEST(HillshadeSlopes, PlaneEncodesAsTheShaderDoes) {
    const auto plane = [](uint32_t x, uint32_t y) {
        return 1000.0 + 3.0 * x + 5.0 * y;
    };
    const DEMData dem(terrarium(4, plane), Tileset::RasterEncoding::Terrarium);
    EXPECT_TRUE(sameEverywhere(HillshadeSlopes::make(dem, 22), {168, 194, 255, 255}));
}

// 196 under z15 the shader steepens the slope, as it does here
TEST(HillshadeSlopes, LowZoomsAreExaggerated) {
    const auto plane = [](uint32_t x, uint32_t y) {
        return 1000.0 + 120.0 * x + 200.0 * y;
    };
    const DEMData dem(terrarium(4, plane), Tileset::RasterEncoding::Terrarium);
    EXPECT_TRUE(sameEverywhere(HillshadeSlopes::make(dem, 12), {130, 132, 255, 255}));
}

// 196 a neighbour's border changes the two outer pixels, and remaking them equals a fresh image
TEST(HillshadeSlopes, RemadeEdgesMatchAFreshImage) {
    DEMData west(terrarium(8, [](auto, auto) { return 1000.0; }), Tileset::RasterEncoding::Terrarium);
    const DEMData east(terrarium(8, [](uint32_t x, auto) { return 1000.0 + 40.0 * x; }),
                       Tileset::RasterEncoding::Terrarium);
    auto slopes = HillshadeSlopes::make(west, 16);
    west.backfillBorder(east, 1, 0);
    HillshadeSlopes::remakeEdges(slopes, west, 16);
    const auto fresh = HillshadeSlopes::make(west, 16);
    EXPECT_EQ(slopes.size, fresh.size);
    for (uint32_t y = 0; y < fresh.size.height; y++) {
        for (uint32_t x = 0; x < fresh.size.width; x++) {
            EXPECT_EQ(at(slopes, x, y), at(fresh, x, y)) << x << "," << y;
        }
    }
    EXPECT_NE(at(fresh, 9, 4)[0], 128) << "the east edge reads the border's rise";
    EXPECT_EQ(at(fresh, 4, 4)[0], 128) << "the middle reads none of it";
}
