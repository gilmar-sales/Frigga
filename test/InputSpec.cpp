#include "EmptyApp.hpp"

#include <Frigga/Asset/AssetRegistry.hpp>
#include <Frigga/Asset/PrimitiveMeshFactory.hpp>
#include <Frigga/Input/Input.hpp>
#include <Frigga/Input/InputMap.hpp>
#include <Frigga/Input/InputMapIO.hpp>
#include <Frigga/Physics/IPhysicsWorld.hpp>
#include <Frigga/Physics/JoltPhysicsWorld.hpp>
#include <Frigga/Scene/Scene.hpp>
#include <Frigga/Scene/SceneSimulationState.hpp>

#include <Freya/Events/EventManager.hpp>
#include <Freya/Events/Gamepad.hpp>
#include <Freya/Events/KeyCode.hpp>
#include <Freyr/Freyr.hpp>
#include <Skirnir/Logging/Logger.hpp>
#include <gtest/gtest.h>

namespace
{
    struct InputHarness
    {
        skr::Arc<skr::IApplication> app;
        skr::Arc<fra::EventManager> events;
        skr::Arc<fg::Input> input;
        skr::Arc<fg::SceneSimulationState> simulation;

        static InputHarness Create()
        {
            InputHarness harness;
            harness.app =
                skr::ApplicationBuilder()
                    .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension &freyr) {
                        freyr.WithPipeline([](fr::PipelineBuilder &pipeline) {
                            pipeline.WithName("Simulation");
                        });
                    })
                    .Build<EmptyApp>();

            const auto services = harness.app->GetRootServiceProvider();
            harness.events      = skr::MakeArc<fra::EventManager>();
            auto registry       = services->GetService<fr::Registry>();
            auto physics        = skr::MakeArc<fg::JoltPhysicsWorld>();
            auto logger =
                skr::MakeArc<skr::Logger<fg::SceneSimulationState>>(skr::MakeArc<skr::LoggerOptions>());
            auto primitives = skr::MakeArc<fg::PrimitiveMeshFactory>(fg::PrimitiveMeshFactory::Catalog);
            auto assets     = skr::MakeArc<fg::AssetRegistry>(fg::AssetRegistry::Catalog);
            auto sceneLogger =
                skr::MakeArc<skr::Logger<fg::Scene>>(skr::MakeArc<skr::LoggerOptions>());
            auto userComponents = skr::MakeArc<fg::UserComponentRegistry>();
            auto scene =
                skr::MakeArc<fg::Scene>(skr::Arc<fra::Renderer> {}, sceneLogger, registry,
                                        primitives, assets, userComponents);

            harness.simulation =
                skr::MakeArc<fg::SceneSimulationState>(registry, physics, scene, primitives, logger);
            harness.input = skr::MakeArc<fg::Input>(harness.events, harness.simulation);
            return harness;
        }
    };
} // namespace

TEST(InputMap, SerializeRoundTripPreservesDefaults)
{
    const auto original = fg::MakeDefaultInputMap();
    const auto json     = fg::SerializeInputMap(original);
    fg::InputMap parsed;
    std::string error;
    ASSERT_TRUE(fg::ParseInputMap(json, parsed, &error)) << error;

    EXPECT_EQ(parsed.version, original.version);
    ASSERT_TRUE(parsed.actions.contains("Jump"));
    ASSERT_TRUE(parsed.actions.contains("Fire"));
    ASSERT_TRUE(parsed.axes.contains("Horizontal"));
    ASSERT_TRUE(parsed.axes.contains("Vertical"));

    EXPECT_FALSE(parsed.actions["Jump"].keys.empty());
    EXPECT_EQ(parsed.axes["Horizontal"].deadzone, 0.15f);
    EXPECT_TRUE(parsed.axes["Vertical"].invertGamepad);
}

TEST(Input, EdgesAndAxes)
{
    auto harness = InputHarness::Create();
    auto &input  = *harness.input;
    input.SetGameplayViewportHovered(true);
    harness.simulation->Play();

    input.BeginFrame();
    EXPECT_FALSE(input.IsDown("Jump"));
    EXPECT_FLOAT_EQ(input.GetAxis("Horizontal"), 0.0f);

    input.InjectKey(fra::KeyCode::D, true);
    input.BeginFrame();
    EXPECT_FLOAT_EQ(input.GetAxis("Horizontal"), 1.0f);

    input.InjectKey(fra::KeyCode::A, true);
    input.BeginFrame();
    EXPECT_FLOAT_EQ(input.GetAxis("Horizontal"), 0.0f);

    input.InjectKey(fra::KeyCode::A, false);
    input.InjectKey(fra::KeyCode::D, false);
    input.InjectGamepadAxis(fra::GamepadAxis::GamepadAxisLeftX, 0.05f);
    input.BeginFrame();
    EXPECT_FLOAT_EQ(input.GetAxis("Horizontal"), 0.0f);

    input.InjectGamepadAxis(fra::GamepadAxis::GamepadAxisLeftX, 0.8f);
    input.BeginFrame();
    EXPECT_NEAR(input.GetAxis("Horizontal"), 0.8f, 0.001f);

    input.InjectKey(fra::KeyCode::Space, true);
    input.BeginFrame();
    EXPECT_TRUE(input.WasPressed("Jump"));
    EXPECT_TRUE(input.IsDown("Jump"));

    input.BeginFrame();
    EXPECT_FALSE(input.WasPressed("Jump"));
    EXPECT_TRUE(input.IsDown("Jump"));

    input.InjectKey(fra::KeyCode::Space, false);
    input.BeginFrame();
    EXPECT_TRUE(input.WasReleased("Jump"));
    EXPECT_FALSE(input.IsDown("Jump"));

    harness.simulation->Pause();
    input.InjectKey(fra::KeyCode::D, true);
    input.BeginFrame();
    EXPECT_FLOAT_EQ(input.GetAxis("Horizontal"), 0.0f);
}
