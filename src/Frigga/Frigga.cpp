#include "Frigga.hpp"

#include "Core/LayerStack.hpp"
#include "ECS/Components/MeshComponent.hpp"
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
            .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension &freyr) {
                freyr.WithComponent<NameComponent>()
                    .WithComponent<TransformComponent>()
                    .WithComponent<MeshComponent>()
                    .WithPipeline([](fr::PipelineBuilder &pipeline) {
                        pipeline.WithName("Main")
                            .WithSystem<PhysicsSystem>()
                            .WithSystem<RenderSystem>();
                    });
            })
            .WithExtension<fra::FreyaExtension>([](fra::FreyaExtension &freya) {
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
