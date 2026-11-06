#include <mln/test/util.hpp>

#include <mln/renderer/terrain_cache_policy.hpp>

using namespace mln::terrain;

TEST(TerrainCachePolicy, RebindsToABetterTier) {
    EXPECT_TRUE(shouldRebind({0, -1}, {1, 6}));  // placeholder -> ancestor
    EXPECT_TRUE(shouldRebind({1, 12}, {2, 13})); // ancestor -> own DEM
    EXPECT_TRUE(shouldRebind({0, -1}, {2, 12})); // placeholder -> own DEM
}

TEST(TerrainCachePolicy, NeverRebindsToAWorseTier) {
    EXPECT_FALSE(shouldRebind({2, 12}, {1, 11}));
    EXPECT_FALSE(shouldRebind({1, 6}, {0, -1}));
}

TEST(TerrainCachePolicy, RebindsToADeeperAncestor) {
    // The regression: a z13 tile first built on the z6 ancestor stayed flat after z12 loaded.
    EXPECT_TRUE(shouldRebind({1, 6}, {1, 12}));
    EXPECT_TRUE(shouldRebind({1, 11}, {1, 12}));
}

TEST(TerrainCachePolicy, KeepsTheSameOrADeeperAncestor) {
    EXPECT_FALSE(shouldRebind({1, 12}, {1, 12}));
    EXPECT_FALSE(shouldRebind({1, 12}, {1, 6})); // a deeper ancestor was evicted: keep what is bound
    EXPECT_FALSE(shouldRebind({2, 12}, {2, 12}));
    EXPECT_FALSE(shouldRebind({0, -1}, {0, -1}));
}

namespace {
std::vector<DrapeGroupFallback> groups(std::initializer_list<int16_t> deficits) {
    std::vector<DrapeGroupFallback> out;
    int32_t layerIndex = 0;
    for (const auto deficit : deficits) {
        out.push_back({layerIndex++, deficit});
    }
    return out;
}
} // namespace

TEST(TerrainCachePolicy, SameDrapeContentIsNotWorse) {
    EXPECT_FALSE(drapeFallbackStrictlyWorse(groups({0, 1, -1}), groups({0, 1, -1})));
}

TEST(TerrainCachePolicy, LosingContentIsWorse) {
    // A tile briefly dropped out of the render set: keep the bake instead of flashing it empty.
    EXPECT_TRUE(drapeFallbackStrictlyWorse(groups({0, -1, 0}), groups({0, 1, 0})));
}

TEST(TerrainCachePolicy, CoarserFallbackIsWorse) {
    EXPECT_TRUE(drapeFallbackStrictlyWorse(groups({0, 3, 0}), groups({0, 1, 0})));
}

TEST(TerrainCachePolicy, AnyGroupImprovingIsNotWorse) {
    // The regression: hillshade moves from a z6 to its z12 DEM (deficit 7 -> 1) while vector groups
    // fall back a level. Summed over groups the new content scored worse (10 -> 16 in the field
    // log) and the hillshade-less bake was kept for the life of the target.
    EXPECT_FALSE(drapeFallbackStrictlyWorse(groups({1, 3, 3, 3}), groups({7, 0, 0, 0})));
    // A group gaining content at all is an improvement too.
    EXPECT_FALSE(drapeFallbackStrictlyWorse(groups({1, 4}), groups({-1, 0})));
}

TEST(TerrainCachePolicy, DifferentGroupSetsAreNotComparable) {
    EXPECT_FALSE(drapeFallbackStrictlyWorse(groups({-1, -1}), groups({0, 0, 0})));
    auto renumbered = groups({-1, -1});
    renumbered[1].layerIndex = 7;
    EXPECT_FALSE(drapeFallbackStrictlyWorse(renumbered, groups({0, 0})));
}

TEST(TerrainCachePolicy, PlaceholderTexelIsSeaLevel) {
    // An all-zero texel decoded to -10000 m and sank the ground of a tile with no DEM out of view.
    const std::array<float, 4> mapbox{{6553.6f, 25.6f, 0.1f, 10000.0f}};
    const std::array<float, 4> terrarium{{256.0f, 1.0f, 1.0f / 256.0f, 32768.0f}};
    EXPECT_EQ(seaLevelTexel(mapbox), (std::array<uint8_t, 3>{{1, 134, 160}}));
    EXPECT_EQ(seaLevelTexel(terrarium), (std::array<uint8_t, 3>{{128, 0, 0}}));
}

namespace {

std::vector<mln::terrain::DrapeGrowth> drapes(std::initializer_list<std::array<uint32_t, 2>> wantedAndCurrent) {
    std::vector<mln::terrain::DrapeGrowth> list;
    double edge = 1000.0;
    for (const auto& [wanted, current] : wantedAndCurrent) {
        list.push_back({.wanted = wanted, .current = current, .edge = edge});
        edge -= 100.0;
    }
    return list;
}

constexpr std::size_t twoLarge = 2u * 1024 * 1024;

} // namespace

// 84 six new drapes at once stalled a frame for 159 ms; the nearest are drawn, the rest wait small
TEST(TerrainDrapeGrowth, ABurstOfNewDrapesDrawsTheNearestFirst) {
    auto list = drapes({{1024, 0}, {1024, 0}, {1024, 0}, {1024, 0}});
    EXPECT_TRUE(mln::terrain::rationDrapeGrowth(list, twoLarge, 512));
    EXPECT_EQ(1024u, list[0].side);
    EXPECT_FALSE(list[0].heldBack);
    EXPECT_EQ(512u, list[2].side);
    EXPECT_TRUE(list[2].heldBack);
    EXPECT_EQ(512u, list[3].side);
}

// 84 a drape changing size keeps its texture until there is room, never going blank
TEST(TerrainDrapeGrowth, AResizeOverTheBudgetKeepsItsTexture) {
    auto list = drapes({{2048, 1024}, {2048, 1024}});
    EXPECT_TRUE(mln::terrain::rationDrapeGrowth(list, twoLarge, 512));
    EXPECT_EQ(2048u, list[0].side);
    EXPECT_EQ(1024u, list[1].side);
    EXPECT_TRUE(list[1].heldBack);
}

// 84 the nearest always grows, so a budget smaller than one drape still makes progress
TEST(TerrainDrapeGrowth, TheNearestGrowsEvenOverTheBudget) {
    auto list = drapes({{4096, 0}, {1024, 0}});
    mln::terrain::rationDrapeGrowth(list, twoLarge, 512);
    EXPECT_EQ(4096u, list[0].side);
    EXPECT_EQ(512u, list[1].side);
}

// 84 frame after frame the held-back drapes reach the size they want
TEST(TerrainDrapeGrowth, HeldBackDrapesGrowOverTheNextFrames) {
    auto list = drapes({{1024, 0}, {1024, 0}, {1024, 0}, {1024, 0}});
    for (int frame = 0; frame < 4; ++frame) {
        mln::terrain::rationDrapeGrowth(list, twoLarge, 512);
        for (auto& drape : list) {
            drape.current = drape.side;
        }
    }
    for (const auto& drape : list) {
        EXPECT_EQ(1024u, drape.side);
        EXPECT_FALSE(drape.heldBack);
    }
}

// 84 nothing new to draw holds nothing back; Quality's 0 draws everything at once
TEST(TerrainDrapeGrowth, NoBudgetOrNothingNewHoldsNothingBack) {
    auto settled = drapes({{1024, 1024}, {2048, 2048}});
    EXPECT_FALSE(mln::terrain::rationDrapeGrowth(settled, twoLarge, 512));
    auto burst = drapes({{1024, 0}, {1024, 0}, {1024, 0}, {1024, 0}});
    EXPECT_FALSE(mln::terrain::rationDrapeGrowth(burst, 0, 512));
    for (const auto& drape : burst) {
        EXPECT_EQ(1024u, drape.side);
    }
}
