#include <mln/renderer/drape_budget.hpp>

#include <gtest/gtest.h>

using namespace mln;

// 158 with nothing set, every mode keeps the budget it had
TEST(DrapeBudget, UnsetKeepsTheModesOwn) {
    for (const auto mode : {TerrainLoadMode::Quality,
                            TerrainLoadMode::Balanced,
                            TerrainLoadMode::Performance,
                            TerrainLoadMode::CloseUp}) {
        EXPECT_EQ(drapeTexelBudget(mode, nullptr), terrainLoadBudget(mode).drapeTexels);
    }
}

// 158 a run can ask for a smaller budget, in millions of texels
TEST(DrapeBudget, MillionsStandInForTheMode) {
    EXPECT_EQ(drapeTexelBudget(TerrainLoadMode::Balanced, "32"), std::size_t{32} << 20);
    EXPECT_EQ(drapeTexelBudget(TerrainLoadMode::Quality, "24"), std::size_t{24} << 20);
}

// 158 a value that is no whole number above zero changes nothing
TEST(DrapeBudget, NonsenseIsIgnored) {
    const auto own = terrainLoadBudget(TerrainLoadMode::Balanced).drapeTexels;
    for (const char* value : {"", "0", "abc", "32MB", "-8"}) {
        EXPECT_EQ(drapeTexelBudget(TerrainLoadMode::Balanced, value), own) << value;
    }
}

// 203 4CA1 a drape is far when its tile is the given levels or more below the nearest; 0 is never
TEST(DrapeBudget, FarDrapesAreLevelsBelowTheNearest) {
    EXPECT_TRUE(isFarDrape(12, 14, 2));
    EXPECT_FALSE(isFarDrape(13, 14, 2));
    EXPECT_FALSE(isFarDrape(14, 14, 2));
    EXPECT_FALSE(isFarDrape(0, 22, 0)) << "0 levels made a drape far";
}

// 203 4CA1 a run can ask for far drapes the map leaves off; a level the map sets wins
TEST(DrapeBudget, ARunCanAskForFarDrapes) {
    EXPECT_EQ(farDrapeLevels(0, "2"), 2);
    EXPECT_EQ(farDrapeLevels(3, "2"), 3);
    for (const char* value : {"", "abc", "2x", "-1", "99"}) {
        EXPECT_EQ(farDrapeLevels(0, value), 0) << value;
    }
    EXPECT_EQ(farDrapeLevels(0, nullptr), 0);
}

// 204 2CB1 kept drapes take what the cover leaves, and an eighth of the budget when it leaves less
TEST(DrapeBudget, KeptDrapesHaveAReserve) {
    EXPECT_EQ(keptDrapeTexels(800, 200), 600u);
    EXPECT_EQ(keptDrapeTexels(800, 750), 100u);
    EXPECT_EQ(keptDrapeTexels(800, 900), 100u) << "a cover over the budget left nothing to keep";
}
