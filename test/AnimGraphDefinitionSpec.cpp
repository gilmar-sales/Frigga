#include <Frigga/Animation/AnimGraphDefinition.hpp>

#include <gtest/gtest.h>

TEST(AnimGraphDefinition, UniqueNameAndRenameUpdatesTransitions)
{
    fg::AnimGraphDefinition graph;
    graph.states.push_back(fg::AnimGraphStateDef {.name = "Idle", .kind = "Clip"});
    graph.entry = "Idle";
    graph.transitions.push_back(
        fg::AnimGraphTransitionDef {.from = "Idle", .to = "Run", .conditionKind = "FloatGreater",
                                    .param = "Speed", .threshold = 0.1f});

    EXPECT_EQ(fg::UniqueAnimGraphStateName(graph, "Idle"), "Idle 2");
    EXPECT_EQ(fg::FindAnimGraphStateIndex(graph, "Idle"), 0);

    fg::RenameAnimGraphState(graph, "Idle", "Wait");
    EXPECT_EQ(graph.states.front().name, "Wait");
    EXPECT_EQ(graph.entry, "Wait");
    EXPECT_EQ(graph.transitions.front().from, "Wait");
}

TEST(AnimGraphDefinition, LayoutAndPopulate)
{
    fg::ModelAsset model {};
    model.relativePath = "Models/Fox.glb";
    model.clips.push_back(fra::AnimationClip {.name = "Idle"});
    model.clips.push_back(fra::AnimationClip {.name = "Run"});

    fg::AnimGraphDefinition graph;
    fg::PopulateAnimGraphFromClips(graph, model);
    ASSERT_EQ(graph.states.size(), 2u);
    EXPECT_EQ(graph.entry, "Idle");
    EXPECT_NE(graph.states[0].editorX, 0.0f);
    EXPECT_TRUE(fg::HasAnimGraphTransition(graph, "Idle", "Run") == false);
}
