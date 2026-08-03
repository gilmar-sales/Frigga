#include "EditorApplication.hpp"
#include "Panels/ArchetypesLayer.hpp"
#include "Panels/EditorLayer.hpp"
#include "Panels/GameplayLayer.hpp"
#include "Panels/HierarchyLayer.hpp"
#include "Panels/PreferencesLayer.hpp"
#include "Panels/ResourcesLayer.hpp"
#include "Preferences/EditorPreferences.hpp"
#include "Preferences/PreferencesStore.hpp"
#include "SelectionContext.hpp"
#include "Workflows/AnimationWorkflow.hpp"
#include "Workflows/AudioWorkflow.hpp"
#include "Workflows/EcsWorkflow.hpp"
#include "Workflows/GamePlayWorkflow.hpp"
#include "Workflows/ScriptingWorkflow.hpp"
#include "Workflows/ShadingWorkflow.hpp"

#include <Frigga/Frigga.hpp>
#include <Frigga/Gui/Styles/Styles.hpp>

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
        builder.SetFullscreen(graphics.fullscreen)
            .SetVSync(graphics.vSync)
            .SetSampleCount(graphics.sampleCount)
            .SetDrawDistance(static_cast<float>(graphics.drawDistance))
            .SetRenderingStrategy(
                static_cast<fra::RenderingStrategy>(graphics.renderingStrategy))
            .SetIblIntensity(static_cast<float>(graphics.iblIntensity))
            .SetExposure(static_cast<float>(graphics.exposure))
            .SetAmbient(glm::vec3(static_cast<float>(graphics.ambientColorR),
                                  static_cast<float>(graphics.ambientColorG),
                                  static_cast<float>(graphics.ambientColorB)),
                        static_cast<float>(graphics.ambientIntensity))
            .SetFrameCount(graphics.frameCount)
            .SetMaxLights(graphics.maxLights)
            .SetEnvironmentMapPath(graphics.environmentMapPath)
            .WithReverseZ(graphics.reverseZ);
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
            [](skr::ServiceProvider &serviceProvider) {
                return serviceProvider.GetService<skr::ConfigurationOptions>()
                    ->Bind<EditorPreferences>();
            })
        .AddSingleton<SelectionContext>()
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
        .AddTransient<ScriptingWorkflow>()
        .AddTransient<EcsWorkflow>();

    auto app = appBuilder.Build<EditorApplication>();

    ApplyTheme(startupPreferences->appearance.themeIndex);

    app->Run();

    return 0;
}
