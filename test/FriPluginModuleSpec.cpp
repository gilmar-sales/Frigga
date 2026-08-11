#include "EmptyApp.hpp"

#include <Frigga/ECS/UserComponentRegistry.hpp>
#include <Frigga/Plugin/FriPluginModule.hpp>

#include <Freyr/Core/SystemManager.hpp>
#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

#include <gtest/gtest.h>

namespace
{
    struct ProbeComponent: fr::Component
    {
        float value = 1.0f;
    };

    std::uint32_t gProbeTicks = 0;

    class ProbeSystem: public fr::System
    {
      public:
        explicit ProbeSystem(const skr::Arc<fr::Registry> &registry) : fr::System(registry) {}

        void Update(float) override
        {
            ++gProbeTicks;
        }
    };

    struct ProbeSingleton
    {
        int marker = 42;
    };

    struct ProbeScoped
    {
        int marker = 7;
    };

    struct ProbeTransient
    {
        int marker = 3;
    };
} // namespace

TEST(FriPluginModule, BuilderRegistersComponentSystemAndDiLifetimes_ThenDetachClears)
{
    gProbeTicks = 0;

    auto app =
        skr::ApplicationBuilder()
            .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension &freyr) {
                freyr.WithPipeline([](fr::PipelineBuilder &pipeline) {
                    pipeline.WithName("Simulation");
                });
            })
            .Build<EmptyApp>();

    const auto services = app->GetRootServiceProvider();
    ASSERT_TRUE(services);

    auto systemManager = services->GetService<fr::SystemManager>();
    auto registry      = services->GetService<fr::Registry>();
    ASSERT_TRUE(systemManager);
    ASSERT_TRUE(registry);

    auto userComponents = skr::MakeArc<fg::UserComponentRegistry>();

    FriPlugin plugin {};
    const FriHost host {
        .registry         = registry.get(),
        .user_components  = userComponents.get(),
        .system_manager   = systemManager.get(),
        .services         = services.get(),
    };

    fg::FriPluginBuilder builder(plugin, host);
    ASSERT_TRUE(builder.IsValid());

    builder.Component<ProbeComponent>()
        .System<ProbeSystem>()
        .Singleton<ProbeSingleton>()
        .Scoped<ProbeScoped>()
        .Transient<ProbeTransient>();

    EXPECT_TRUE(userComponents->Has("ProbeComponent"));
    EXPECT_TRUE(systemManager->IsSystemRegistered<ProbeSystem>());
    EXPECT_TRUE(services->Contains<ProbeSystem>());
    EXPECT_TRUE(services->Contains<ProbeSingleton>());
    EXPECT_TRUE(services->Contains<ProbeScoped>());
    EXPECT_TRUE(services->Contains<ProbeTransient>());

    registry->Update(1.0f / 60.0f);
    EXPECT_GE(gProbeTicks, 1u);

    const auto ticksAfterRun = gProbeTicks;
    plugin.runtime.Detach();

    EXPECT_FALSE(systemManager->IsSystemRegistered<ProbeSystem>());
    EXPECT_FALSE(services->Contains<ProbeSystem>());
    EXPECT_FALSE(services->Contains<ProbeSingleton>());
    EXPECT_FALSE(services->Contains<ProbeScoped>());
    EXPECT_FALSE(services->Contains<ProbeTransient>());

    registry->Update(1.0f / 60.0f);
    EXPECT_EQ(gProbeTicks, ticksAfterRun);
}
