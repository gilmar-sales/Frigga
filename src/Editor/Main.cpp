#include "EditorApplication.hpp"
#include "HomeLayer.hpp"
#include "Panels/ArchetypesLayer.hpp"
#include "Panels/EditorLayer.hpp"
#include "Panels/GameplayLayer.hpp"
#include "Panels/HierarchyLayer.hpp"
#include "Panels/PreferencesLayer.hpp"
#include "Panels/ResourcesLayer.hpp"
#include "Preferences/EditorPreferences.hpp"
#include "Preferences/PreferencesStore.hpp"
#include "Project/ProjectSession.hpp"
#include "SelectionContext.hpp"
#include "Workflows/AnimationWorkflow.hpp"
#include "Workflows/AudioWorkflow.hpp"
#include "Workflows/EcsWorkflow.hpp"
#include "Workflows/GamePlayWorkflow.hpp"
#include "Workflows/ShadingWorkflow.hpp"

#include <Frigga/Frigga.hpp>
#include <Frigga/Gui/Styles/Styles.hpp>

#include <algorithm>

namespace
{
    void ApplyTheme(int themeIndex)
    {
        switch(themeIndex)
        {
        case 0:
            fg::StylePhantomDark();
            break;
        case 1:
            fg::StylePhantomLight();
            break;
        case 2:
            ImGui::StyleColorsDark();
            break;
        case 3:
            ImGui::StyleColorsLight();
            break;
        case 4:
            ImGui::StyleColorsClassic();
            break;
        default:
            fg::StylePhantomDark();
            break;
        }
    }

    void ApplyGraphicsPreferences(fra::FreyaOptionsBuilder &builder,
                                  const GraphicsPreferences &graphics)
    {
        const auto clampQuality = [](int value) { return std::clamp(value, 0, 4); };
        // Startup / edit session uses the Editor viewport profile.
        const auto &viewport = graphics.editorViewport;
        const auto shadowQuality =
            static_cast<fra::ShadowQuality>(clampQuality(viewport.shadowQuality));
        const auto ssaoQuality =
            static_cast<fra::SsaoQuality>(clampQuality(viewport.ssaoQuality));
        const auto taaQuality =
            static_cast<fra::TaaQuality>(clampQuality(viewport.taaQuality));
        const auto bloomQuality =
            static_cast<fra::BloomQuality>(clampQuality(viewport.bloomQuality));
        const auto ssaoDebugView = static_cast<fra::SsaoDebugView>(
            std::clamp(graphics.ssaoDebugView, 0, 2));

        builder.SetTitle(graphics.title)
            .SetWidth(graphics.width)
            .SetHeight(graphics.height)
            .SetFullscreen(graphics.fullscreen)
            .SetVSync(graphics.vSync)
            .SetFrameCount(graphics.frameCount)
            .SetClearColor(vk::ClearColorValue {
                static_cast<float>(graphics.clearColorR),
                static_cast<float>(graphics.clearColorG),
                static_cast<float>(graphics.clearColorB),
                static_cast<float>(graphics.clearColorA),
            })
            .SetDrawDistance(static_cast<float>(graphics.drawDistance))
            .SetMaxLights(graphics.maxLights)
            .SetIblIntensity(static_cast<float>(graphics.iblIntensity))
            .SetExposure(static_cast<float>(graphics.exposure))
            .SetAmbient(glm::vec3(static_cast<float>(graphics.ambientColorR),
                                  static_cast<float>(graphics.ambientColorG),
                                  static_cast<float>(graphics.ambientColorB)),
                        static_cast<float>(graphics.ambientIntensity))
            .SetEnvironmentMapPath(graphics.environmentMapPath)
            .SetShaderRoot(graphics.shaderRoot)
            .SetShadowQuality(shadowQuality)
            .SetSsaoQuality(ssaoQuality)
            .SetTaaQuality(taaQuality)
            .SetBloomQuality(bloomQuality)
            .SetSsaoRadius(static_cast<float>(graphics.ssaoRadius))
            .SetSsaoBias(static_cast<float>(graphics.ssaoBias))
            .SetSsaoPower(static_cast<float>(graphics.ssaoPower))
            .SetSsaoIntensity(static_cast<float>(graphics.ssaoIntensity))
            .SetSsaoDebugView(ssaoDebugView)
            .WithReverseZ(graphics.reverseZ)
            .SetAnimationQuality(fra::AnimationQuality::High);
    }
} // namespace

int main(int argc, char *argv[])
{
    // Early Bind so Freya/Freyr WithOptions can apply values before Build().
    const auto startupPreferences = PreferencesStore::Load();

    auto appBuilder =
        skr::ApplicationBuilder()
            .WithConfiguration([](skr::ConfigurationBuilder &configurationBuilder) {
                PreferencesStore::Configure(configurationBuilder);
            })
            .WithExtension<skr::LoggingExtension>([](skr::LoggingExtension &logging) {
                logging.AddConsoleSink().AddFileSink("frigga.log");
            })
            .WithExtension<fg::FriggaExtension>()
            .WithExtension<fra::FreyaExtension>([&](fra::FreyaExtension &freya) {
                freya.WithOptions([&](fra::FreyaOptionsBuilder &builder) {
                    ApplyGraphicsPreferences(builder, startupPreferences->graphics);
                });
            })
            .WithExtension<fr::FreyrExtension>([&](fr::FreyrExtension &freyr) {
                freyr.WithOptions([&](fr::FreyrOptionsBuilder &builder) {
                    builder.WithMaxEntities(startupPreferences->ecs.maxEntities)
                        .WithArchetypeChunkCapacity(
                            startupPreferences->ecs.archetypeChunkCapacity)
                        .WithThreadCount(startupPreferences->ecs.threadCount);
                });
            });

    appBuilder.GetServiceCollection()
        ->AddSingleton<EditorPreferences>(
            [startupPreferences](skr::ServiceProvider &) { return startupPreferences; })
        .AddSingleton<SelectionContext>()
        .AddSingleton<ProjectSession>()
        .AddTransient<HomeLayer>()
        .AddTransient<MainLayer>()
        .AddTransient<GameplayLayer>()
        .AddTransient<EditorLayer>()
        .AddTransient<ResourcesLayer>()
        .AddTransient<ArchetypesLayer>()
        .AddScoped<HierarchyLayer>()
        .AddTransient<PreferencesLayer>()
        .AddTransient<GamePlayWorkflow>()
        .AddTransient<AnimationWorkflow>()
        .AddTransient<AudioWorkflow>()
        .AddTransient<ShadingWorkflow>()
        .AddTransient<EcsWorkflow>();

    auto app = appBuilder.Build<EditorApplication>();

    ApplyTheme(startupPreferences->appearance.themeIndex);

    app->Run();

    return 0;
}
