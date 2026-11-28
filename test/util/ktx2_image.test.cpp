#include <mln/util/io.hpp>
#include <mln/util/ktx2_image.hpp>

#include <gtest/gtest.h>

#include <cstdlib>

using namespace mln;

namespace {

// The fixtures are 8 × 8: the top four rows (200, 40, 40), the bottom four (40, 40, 200).
void expectHalves(const PremultipliedImage& image, int tolerance) {
    ASSERT_EQ(Size(8, 8), image.size);
    for (uint32_t y : {0u, 3u, 4u, 7u}) {
        const uint8_t* px = image.data.get() + (y * 8 + 3) * 4;
        const int red = y < 4 ? 200 : 40;
        const int blue = y < 4 ? 40 : 200;
        EXPECT_NEAR(red, px[0], tolerance) << "row " << y;
        EXPECT_NEAR(40, px[1], tolerance) << "row " << y;
        EXPECT_NEAR(blue, px[2], tolerance) << "row " << y;
        EXPECT_EQ(255, px[3]) << "row " << y;
    }
}

} // namespace

// 205 A KTX2 file is told from a PNG by its first twelve bytes.
TEST(KTX2Image, IsToldFromOtherImages) {
    EXPECT_TRUE(util::isKTX2(util::read_file("test/fixtures/ktx2/halves-uastc.ktx2")));
    EXPECT_FALSE(util::isKTX2(util::read_file("test/fixtures/ktx2/halves.png")));
    EXPECT_FALSE(util::isKTX2(""));
}

// 205 Both of Basis's formats decode to the photo's pixels where the GPU takes no ASTC.
TEST(KTX2Image, DecodesToRGBA) {
    for (const char* name : {"halves-uastc", "halves-etc1s"}) {
        SCOPED_TRACE(name);
        auto decoded = util::decodeKTX2(util::read_file(std::string("test/fixtures/ktx2/") + name + ".ktx2"), false);
        ASSERT_TRUE(std::holds_alternative<PremultipliedImage>(decoded));
        expectHalves(std::get<PremultipliedImage>(decoded), 12);
    }
}

// 205 For a GPU that takes ASTC, a tile becomes its 4 × 4 blocks, a quarter of RGBA's bytes.
TEST(KTX2Image, TranscodesToASTCBlocks) {
    auto decoded = util::decodeKTX2(util::read_file("test/fixtures/ktx2/halves-uastc.ktx2"), true);
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<ASTCImage>>(decoded));
    const auto& astc = *std::get<std::shared_ptr<ASTCImage>>(decoded);
    EXPECT_EQ(Size(8, 8), astc.size);
    EXPECT_EQ(8u * 8u * 4u / 4u, astc.bytes());
}

// 205 A truncated KTX2 file throws, so the tile fails like a broken PNG does.
TEST(KTX2Image, ATruncatedFileThrows) {
    const auto data = util::read_file("test/fixtures/ktx2/halves-uastc.ktx2");
    EXPECT_THROW(util::decodeKTX2(data.substr(0, 40), false), std::runtime_error);
}
