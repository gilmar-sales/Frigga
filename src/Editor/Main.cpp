#include "EditorApplication.hpp"
#include "Panels/HierarchyLayer.hpp"
#include "Panels/PreferencesLayer.hpp"
#include "Panels/ResourcesLayer.hpp"
#include "Workflows/EcsWorkflow.hpp"
#include "Workflows/GamePlayWorkflow.hpp"

#include <Frigga/Frigga.hpp>

int main(int argc, char *argv[])
{
    auto appBuilder = skr::ApplicationBuilder()
                          .AddExtension<fg::FriggaExtension>()
                          .AddExtension<fra::FreyaExtension>([](fra::FreyaExtension &freya) {
                              freya.WithOptions([](fra::FreyaOptionsBuilder &freyaOptionsBuilder) {
                                  freyaOptionsBuilder.SetFullscreen(false);
                              });
                          });

    appBuilder.GetServiceCollection()
        .AddTransient<MainLayer>()
        .AddTransient<ResourcesLayer>()
        .AddTransient<HierarchyLayer>()
        .AddTransient<PreferencesLayer>()
        .AddTransient<GamePlayWorkflow>()
        .AddTransient<EcsWorkflow>();

    auto app = appBuilder.Build<EditorApplication>();

    app->Run();

    return 0;
}
