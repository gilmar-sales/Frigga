#include "EditorApplication.hpp"
#include "Panels/EditorLayer.hpp"
#include "Panels/GameplayLayer.hpp"
#include "Panels/HierarchyLayer.hpp"
#include "Panels/PreferencesLayer.hpp"
#include "Panels/ResourcesLayer.hpp"
#include "SelectionContext.hpp"
#include "Workflows/EcsWorkflow.hpp"
#include "Workflows/GamePlayWorkflow.hpp"

#include <Frigga/Frigga.hpp>

int main(int argc, char *argv[])
{
    auto appBuilder = skr::ApplicationBuilder()
                          .WithExtension<fg::FriggaExtension>()
                          .WithExtension<fra::FreyaExtension>([](fra::FreyaExtension &freya) {
                              freya.WithOptions([](fra::FreyaOptionsBuilder &freyaOptionsBuilder) {
                                  freyaOptionsBuilder.SetFullscreen(false);
                              });
                          });

    appBuilder.GetServiceCollection()
        ->AddSingleton<SelectionContext>()
        .AddTransient<MainLayer>()
        .AddTransient<GameplayLayer>()
        .AddTransient<EditorLayer>()
        .AddTransient<ResourcesLayer>()
        .AddScoped<HierarchyLayer>()
        .AddTransient<PreferencesLayer>()
        .AddTransient<GamePlayWorkflow>()
        .AddTransient<EcsWorkflow>();

    auto app = appBuilder.Build<EditorApplication>();

    app->Run();

    return 0;
}
