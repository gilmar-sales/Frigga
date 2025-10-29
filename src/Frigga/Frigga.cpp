#include "Frigga.hpp"

#include "Core/LayerStack.hpp"
#include "Scene/Scene.hpp"

#include "ECS/Components/NameComponent.hpp"
#include "ECS/Components/TransformComponent.hpp"
#include "ECS/Systems/PhysicsSystem.hpp"
#include "ECS/Systems/RenderSystem.hpp"

namespace FRIGGA_NAMESPACE
{
    void FriggaExtension::Attach(skr::ApplicationBuilder &applicationBuilder)
    {
        applicationBuilder
            .AddExtension<fr::FreyrExtension>([](fr::FreyrExtension &freyr) {
                freyr.AddComponent<NameComponent>()
                    .AddComponent<TransformComponent>()
                    .AddSystem<PhysicsSystem>()
                    .AddSystem<RenderSystem>();
            })
            .AddExtension<fra::FreyaExtension>([](fra::FreyaExtension &freya) {
                freya.WithOptions([](fra::FreyaOptionsBuilder &freyaOptionsBuilder) {
                    freyaOptionsBuilder.SetTitle("Frigga Application")
                        .SetVSync(true)
                        .SetWidth(1280)
                        .SetHeight(720);
                });
            });
    }

    void FriggaExtension::ConfigureServices(skr::ServiceCollection &services)
    {
        services.AddScoped<fg::Scene>();
        services.AddScoped<fg::LayerStack>();
        services.AddTransient<fg::GuiLayer>();
    }
} // namespace FRIGGA_NAMESPACE