#include <mln/test/util.hpp>

#include <mln/map/transform_state.hpp>
#include <mln/renderer/placement_pace.hpp>

using namespace mln;

// 110 A pan re-placed every symbol layer nearly every frame as its tiles came in.
TEST(PlacementPace, NewSymbolsWaitWhileTheCameraMoves) {
    TransformState state;
    EXPECT_TRUE(placesNewSymbolsSoon(true, state));
    EXPECT_FALSE(placesNewSymbolsSoon(false, state));

    state.setPanningInProgress(true);
    EXPECT_FALSE(placesNewSymbolsSoon(true, state));
    state.setPanningInProgress(false);

    state.setScalingInProgress(true);
    EXPECT_FALSE(placesNewSymbolsSoon(true, state));
    state.setScalingInProgress(false);

    state.setGestureInProgress(true);
    EXPECT_FALSE(placesNewSymbolsSoon(true, state));
    state.setGestureInProgress(false);

    EXPECT_TRUE(placesNewSymbolsSoon(true, state));
}

// 110 A tipped view keeps upstream's pace: its new symbols are far away.
TEST(PlacementPace, ATippedViewPlacesAtTheUsualPace) {
    TransformState state;
    state.setProperties(TransformStateProperties().withPitch(0.5));
    EXPECT_FALSE(placesNewSymbolsSoon(true, state));
}
