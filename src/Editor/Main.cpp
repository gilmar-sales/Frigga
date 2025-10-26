#include "EditorApplication.hpp"

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

    appBuilder.GetServiceCollection().AddTransient<MainLayer>();

    auto app = appBuilder.Build<EditorApplication>();

    app->Run();

    return 0;
}
