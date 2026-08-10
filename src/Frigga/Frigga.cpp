#include "Frigga.hpp"

#include "Animation/AnimationController.hpp"
#include "Asset/AssetRegistry.hpp"
#include "Asset/PrimitiveMeshFactory.hpp"
#include "Core/LayerStack.hpp"
#include "ECS/Components/AnimatorComponent.hpp"
#include "ECS/Components/CameraComponent.hpp"
#include "ECS/Components/LightComponent.hpp"
#include "ECS/Components/MaterialComponent.hpp"
#include "ECS/Components/MeshComponent.hpp"
#include "ECS/Components/NameComponent.hpp"
#include "ECS/Components/RigidBodyComponent.hpp"
#include "ECS/Components/TransformComponent.hpp"
#include "ECS/Systems/AnimationSystem.hpp"
#include "ECS/Systems/PhysicsSystem.hpp"
#include "ECS/Systems/RenderSystem.hpp"
#include "ECS/UserComponentRegistry.hpp"
#include "Physics/IPhysicsWorld.hpp"
#include "Physics/JoltPhysicsWorld.hpp"
#include "Plugin/GameplayPluginBridge.hpp"
#include "Plugin/GameplayPluginHost.hpp"
#include "Scene/Scene.hpp"
#include "Scene/SceneSimulationState.hpp"

namespace FRIGGA_NAMESPACE
{
    void FriggaExtension::Attach(skr::ApplicationBuilder &applicationBuilder)
    {
        applicationBuilder
            .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension &freyr) {
                freyr.WithComponent<NameComponent>()
                    .WithComponent<TransformComponent>()
                    .WithComponent<MeshComponent>()
                    .WithComponent<MaterialComponent>()
                    .WithComponent<CameraComponent>()
                    .WithComponent<LightComponent>()
                    .WithComponent<RigidBodyComponent>()
                    .WithComponent<AnimatorComponent>()
                    .WithPipeline([](fr::PipelineBuilder &pipeline) {
                        pipeline.WithName("Main")
                            .WithSystem<PhysicsSystem>()
                            .WithSystem<AnimationSystem>()
                            .WithSystem<GameplayPluginBridge>()
                            .WithSystem<RenderSystem>();
                    });
            })
            .WithExtension<fra::FreyaExtension>([](fra::FreyaExtension &freya) {
                freya.WithOptions([](fra::FreyaOptionsBuilder &freyaOptionsBuilder) {
                    freyaOptionsBuilder.SetTitle("Frigga Application")
                        .SetVSync(true)
                        .SetWidth(1280)
                        .SetHeight(720)
                        .SetAnimationQuality(fra::AnimationQuality::High);
                });
            });
    }

    void FriggaExtension::ConfigureServices(skr::ServiceCollection &services)
    {
        services.AddSingleton<fg::PrimitiveMeshFactory>();
        services.AddSingleton<fg::AssetRegistry>();
        services.AddSingleton<fg::AnimationController>();
        services.AddSingleton<fg::IPhysicsWorld, fg::JoltPhysicsWorld>();
        services.AddSingleton<fg::Scene>();
        services.AddSingleton<fg::SceneSimulationState>();
        services.AddSingleton<fg::UserComponentRegistry>();
        services.AddSingleton<fg::GameplayPluginHost>();
        services.AddScoped<fg::LayerStack>();
        services.AddSingleton<fg::GuiLayer>();
    }
} // namespace FRIGGA_NAMESPACE
