#include <mln/test/util.hpp>

#include <mln/renderer/buckets/symbol_bucket.hpp>
#include <mln/style/conversion/json.hpp>
#include <mln/style/conversion/layer.hpp>
#include <mln/style/layers/symbol_layer.hpp>

using namespace mln;
using namespace mln::style;

namespace {

std::unique_ptr<Layer> symbolLayer(const std::string& zOffset) {
    conversion::Error error;
    auto layer = conversion::convertJSON<std::unique_ptr<Layer>>(
        R"({"id": "numbers", "type": "symbol", "source": "s", "layout": {"symbol-z-offset": )" + zOffset + "}}",
        error);
    return layer ? std::move(*layer) : nullptr;
}

#if MLN_USE_SYMBOL_INSTANCING
uint16_t packedDecimetres(float zOffset) {
    const SymbolQuad quad({0, 0}, {1, 0}, {0, 1}, {1, 1}, {0, 0, 1, 1}, WritingModeType::Horizontal, {0, 0}, true,
                          {0, 0}, {0, 0}, {0, 0});
    const Anchor anchor(10, 20, 0, 0);
    return SymbolBucket::layoutAttributes(quad, anchor, {12, 12}, zOffset).a6[2];
}
#endif

} // namespace

// 80 a house number takes its building's height from the feature
TEST(SymbolZOffset, ParsesAsDataDriven) {
    auto layer = symbolLayer(R"(["get", "render_height"])");
    ASSERT_NE(nullptr, layer);
    const auto& zOffset = static_cast<SymbolLayer&>(*layer).getSymbolZOffset();
    ASSERT_TRUE(zOffset.isExpression());
    EXPECT_FALSE(zOffset.asExpression().isFeatureConstant());
}

// 80 without the property a label stays on the ground
TEST(SymbolZOffset, DefaultsToTheGround) {
    EXPECT_EQ(0.0f, SymbolLayer::getDefaultSymbolZOffset().asConstant());
    auto layer = symbolLayer("3");
    ASSERT_NE(nullptr, layer);
    EXPECT_EQ(3.0f, static_cast<SymbolLayer&>(*layer).getSymbolZOffset().asConstant());
}

#if MLN_USE_SYMBOL_INSTANCING
// 80 the height reaches the shader in decimetres, never below the ground
TEST(SymbolZOffset, PacksDecimetresIntoTheInstance) {
    EXPECT_EQ(95, packedDecimetres(9.46f));
    EXPECT_EQ(0, packedDecimetres(0.0f));
    EXPECT_EQ(0, packedDecimetres(-4.0f));
    EXPECT_EQ(65535, packedDecimetres(7000.0f));
}
#endif
