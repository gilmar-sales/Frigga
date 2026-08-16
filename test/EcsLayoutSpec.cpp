#include "EmptyApp.hpp"

#include <Frigga/ECS/EcsLayout.hpp>

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <vector>

namespace
{
    std::uint32_t gProbeTicks = 0;

    class LayoutProbeSystem: public fr::System
    {
      public:
        explicit LayoutProbeSystem(const skr::Arc<fr::Registry> &registry) : fr::System(registry) {}

        void Update(float) override
        {
            ++gProbeTicks;
        }
    };

    [[nodiscard]] std::string SystemLabelOf(fr::Registry &registry, fr::SystemId id)
    {
        std::string label;
        registry.ForEachRegisteredSystem([&](fr::SystemId systemId, std::string_view name) {
            if(systemId == id)
            {
                label = std::string(name);
            }
        });
        return label;
    }
} // namespace

TEST(EcsLayout, ParseSerializeRoundTrip)
{
    fg::EcsLayout layout {};
    layout.defaultPipeline = "Simulation";
    layout.pipelines.push_back(fg::EcsPipelineLayout {
        .name = "Render", .hz = 0.0f, .enabled = true, .systems = {"AnimationSystem", "RenderSystem"}});
    layout.pipelines.push_back(fg::EcsPipelineLayout {
        .name     = "Simulation",
        .hz       = 60.0f,
        .enabled  = true,
        .systems  = {"GameplayPluginBridge", "PhysicsSystem"},
    });

    std::string json;
    ASSERT_TRUE(fg::SerializeEcsLayout(layout, json));

    fg::EcsLayout parsed {};
    ASSERT_TRUE(fg::ParseEcsLayout(json, parsed));
    EXPECT_EQ(parsed.defaultPipeline, "Simulation");
    ASSERT_EQ(parsed.pipelines.size(), 2u);
    EXPECT_EQ(parsed.pipelines[1].hz, 60.0f);
    ASSERT_EQ(parsed.pipelines[1].systems.size(), 2u);
    EXPECT_EQ(parsed.pipelines[1].systems[0], "GameplayPluginBridge");
}

TEST(EcsLayout, EnforceLocksSimulationAt60AndPinsRenderLast)
{
    auto app =
        skr::ApplicationBuilder()
            .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension &freyr) {
                freyr.WithPipeline([](fr::PipelineBuilder &pipeline) {
                    pipeline.WithName("Main");
                });
                freyr.WithPipeline([](fr::PipelineBuilder &pipeline) {
                    pipeline.WithName("Simulation");
                });
            })
            .Build<EmptyApp>();

    auto registry = app->GetRootServiceProvider()->GetService<fr::Registry>();
    ASSERT_TRUE(registry);

    fg::EcsLayout layout {};
    layout.pipelines.push_back(
        fg::EcsPipelineLayout {.name = "Main", .hz = 0.0f, .enabled = true, .systems = {}});
    layout.pipelines.push_back(
        fg::EcsPipelineLayout {.name = "Simulation", .hz = 0.0f, .enabled = true, .systems = {}});
    layout.pipelines.push_back(
        fg::EcsPipelineLayout {.name = "Logic", .hz = 30.0f, .enabled = true, .systems = {}});

    ASSERT_TRUE(fg::ApplyEcsLayout(*registry, layout).ok);

    const auto simId = registry->FindPipelineId("Simulation");
    ASSERT_TRUE(simId);
    EXPECT_NEAR(fg::StoredRateToHz(registry->GetPipeline(*simId).Rate), fg::kSimulationRateHz,
                0.01f);

    const auto renderId = registry->FindPipelineId("Render");
    ASSERT_TRUE(renderId);

    std::vector<std::string> names;
    registry->ForEachPipeline(
        [&](const fr::PipelineView &pipe) { names.emplace_back(pipe.Name); });
    ASSERT_FALSE(names.empty());
    EXPECT_EQ(names.back(), "Render");
    EXPECT_TRUE(std::find(names.begin(), names.end(), "Logic") != names.end());
}

TEST(EcsLayout, ApplyMovesUnknownSystemsToSimulationLast)
{
    auto app =
        skr::ApplicationBuilder()
            .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension &freyr) {
                freyr.WithPipeline([](fr::PipelineBuilder &pipeline) {
                    pipeline.WithName("Main");
                });
                freyr.WithPipeline([](fr::PipelineBuilder &pipeline) {
                    pipeline.WithName("Simulation");
                });
            })
            .Build<EmptyApp>();

    const auto services = app->GetRootServiceProvider();
    auto registry       = services->GetService<fr::Registry>();
    ASSERT_TRUE(registry);

    const auto simulationId = registry->FindPipelineId("Simulation");
    ASSERT_TRUE(simulationId);
    services->AddSingleton<LayoutProbeSystem>();
    registry->RegisterSystem<LayoutProbeSystem>(*simulationId);

    std::string probeLabel;
    registry->ForEachRegisteredSystem([&](fr::SystemId, std::string_view name) {
        if(name.find("LayoutProbeSystem") != std::string_view::npos)
        {
            probeLabel = std::string(name);
        }
    });
    ASSERT_FALSE(probeLabel.empty());

    fg::EcsLayout layout {};
    layout.pipelines.push_back(
        fg::EcsPipelineLayout {.name = "Main", .enabled = true, .systems = {}});
    layout.pipelines.push_back(
        fg::EcsPipelineLayout {.name = "Simulation", .enabled = true, .systems = {}});

    const auto result = fg::ApplyEcsLayout(*registry, layout);
    ASSERT_TRUE(result.ok);
    EXPECT_TRUE(result.addedPluginSystems);

    const auto sim = registry->GetPipeline(*simulationId);
    ASSERT_FALSE(sim.Systems.empty());
    EXPECT_EQ(SystemLabelOf(*registry, sim.Systems.back()), probeLabel);
}

TEST(EcsLayout, SyncCreatesMissingFileFromRuntime)
{
    auto app =
        skr::ApplicationBuilder()
            .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension &freyr) {
                freyr.WithPipeline([](fr::PipelineBuilder &pipeline) {
                    pipeline.WithName("Main");
                });
                freyr.WithPipeline([](fr::PipelineBuilder &pipeline) {
                    pipeline.WithName("Simulation");
                });
            })
            .Build<EmptyApp>();

    auto registry = app->GetRootServiceProvider()->GetService<fr::Registry>();
    ASSERT_TRUE(registry);

    const auto path =
        std::filesystem::temp_directory_path() / "frigga_ecs_layout_sync_test.json";
    std::error_code ec;
    std::filesystem::remove(path, ec);

    const auto result = fg::SyncEcsLayoutFile(*registry, path);
    ASSERT_TRUE(result.ok);
    ASSERT_TRUE(std::filesystem::exists(path));

    fg::EcsLayout loaded {};
    ASSERT_TRUE(fg::LoadEcsLayoutFile(path, loaded));
    EXPECT_GE(loaded.pipelines.size(), 3u);
    ASSERT_FALSE(loaded.pipelines.empty());
    EXPECT_EQ(loaded.pipelines.back().name, "Render");
    bool foundSim = false;
    for(const auto &pipe : loaded.pipelines)
    {
        if(pipe.name == "Simulation")
        {
            foundSim = true;
            EXPECT_NEAR(pipe.hz, fg::kSimulationRateHz, 0.01f);
        }
    }
    EXPECT_TRUE(foundSim);

    std::filesystem::remove(path, ec);
}
