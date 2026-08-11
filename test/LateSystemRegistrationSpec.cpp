#include "EmptyApp.hpp"

#include <Freyr/Core/SystemManager.hpp>
#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

#include <gtest/gtest.h>

namespace
{
    std::uint32_t gLateSystemTicks = 0;

    class LateProbeSystem: public fr::System
    {
      public:
        explicit LateProbeSystem(const skr::Arc<fr::Registry> &registry) : fr::System(registry) {}

        void Update(float) override
        {
            ++gLateSystemTicks;
        }
    };
} // namespace

TEST(LateSystemRegistration, HostDiAddSingleton_RegisterSystem_TicksThenUnregisters)
{
    gLateSystemTicks = 0;

    auto app =
        skr::ApplicationBuilder()
            .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension &freyr) {
                freyr.WithPipeline([](fr::PipelineBuilder &pipeline) {
                    pipeline.WithName("Main");
                });
            })
            .Build<EmptyApp>();

    const auto services = app->GetRootServiceProvider();
    ASSERT_TRUE(services);

    auto systemManager = services->GetService<fr::SystemManager>();
    auto registry      = services->GetService<fr::Registry>();
    ASSERT_TRUE(systemManager);
    ASSERT_TRUE(registry);

    const auto pipelineId = systemManager->FindPipelineId("Main");
    ASSERT_TRUE(pipelineId.has_value());

    services->AddSingleton<LateProbeSystem>();
    systemManager->RegisterSystem<LateProbeSystem>(*pipelineId);
    EXPECT_TRUE(systemManager->IsSystemRegistered<LateProbeSystem>());

    registry->Update(1.0f / 60.0f);
    EXPECT_GE(gLateSystemTicks, 1u);

    const auto ticksAfterRun = gLateSystemTicks;
    EXPECT_TRUE(systemManager->UnregisterSystem<LateProbeSystem>());
    EXPECT_TRUE(services->Remove<LateProbeSystem>());
    EXPECT_FALSE(systemManager->IsSystemRegistered<LateProbeSystem>());

    registry->Update(1.0f / 60.0f);
    EXPECT_EQ(gLateSystemTicks, ticksAfterRun);
}
