#include "Frigga.hpp"

#include "Animation/AnimationController.hpp"
#include "Asset/AssetRegistry.hpp"
#include "Asset/PrimitiveMeshFactory.hpp"
#include "Core/LayerStack.hpp"
#include "Gui/GuiLayer.hpp"
#include "ECS/Components/AnimatorComponent.hpp"
#include "ECS/Components/BillboardComponent.hpp"
#include "ECS/Components/BillboardTextComponent.hpp"
#include "ECS/Components/CameraComponent.hpp"
#include "ECS/Components/FullscreenEffectComponent.hpp"
#include "ECS/Components/HealthBarComponent.hpp"
#include "ECS/Components/HierarchyComponent.hpp"
#include "ECS/Components/LightComponent.hpp"
#include "ECS/Components/MaterialComponent.hpp"
#include "ECS/Components/MeshComponent.hpp"
#include "ECS/Components/NameComponent.hpp"
#include "ECS/Components/NetworkIdentity.hpp"
#include "ECS/Components/NetworkTransform.hpp"
#include "ECS/Components/ParticleEmitterComponent.hpp"
#include "ECS/Components/RigidBodyComponent.hpp"
#include "ECS/Components/TransformComponent.hpp"
#include "ECS/Systems/AnimationSystem.hpp"
#include "ECS/Systems/NetworkReceiveSystem.hpp"
#include "ECS/Systems/NetworkSendSystem.hpp"
#include "ECS/Systems/PhysicsSystem.hpp"
#include "ECS/Systems/RenderSystem.hpp"
#include "ECS/UserComponentRegistry.hpp"
#include "Input/Input.hpp"
#include "Net/Network.hpp"
#include "Physics/IPhysicsWorld.hpp"
#include "Physics/JoltPhysicsWorld.hpp"
#include "Physics/Physics.hpp"
#include "Plugin/GameplayPluginBridge.hpp"
#include "Plugin/GameplayPluginHost.hpp"
#include "Scene/Scene.hpp"
#include "Scene/SceneSimulationState.hpp"

#include <Freya/Events/EventManager.hpp>

namespace FRIGGA_NAMESPACE
{
    void FriggaExtension::Attach(skr::ApplicationBuilder &applicationBuilder)
    {
        const bool headless = mHeadless;
        applicationBuilder.WithExtension<fr::FreyrExtension>([headless](fr::FreyrExtension &freyr) {
            freyr.WithComponent<NameComponent>()
                .WithComponent<HierarchyComponent>()
                .WithComponent<TransformComponent>()
                .WithComponent<MeshComponent>()
                .WithComponent<MaterialComponent>()
                .WithComponent<CameraComponent>()
                .WithComponent<LightComponent>()
                .WithComponent<RigidBodyComponent>()
                .WithComponent<BillboardComponent>()
                .WithComponent<BillboardTextComponent>()
                .WithComponent<HealthBarComponent>()
                .WithComponent<ParticleEmitterComponent>()
                .WithComponent<FullscreenEffectComponent>()
                .WithComponent<AnimatorComponent>()
                .WithComponent<NetworkIdentity>()
                .WithComponent<NetworkTransform>()
                .WithPipeline([](fr::PipelineBuilder &pipeline) {
                    pipeline.WithName("Simulation")
                        .WithRate(60)
                        .WithSystem<NetworkReceiveSystem>()
                        .WithSystem<GameplayPluginBridge>()
                        .WithSystem<PhysicsSystem>()
                        .WithSystem<NetworkSendSystem>();
                })
                .WithPipeline([](fr::PipelineBuilder &pipeline) {
                    pipeline.WithName("Main");
                });
            if(!headless)
            {
                freyr.WithPipeline([](fr::PipelineBuilder &pipeline) {
                    pipeline.WithName("Render")
                        .WithSystem<AnimationSystem>()
                        .WithSystem<RenderSystem>();
                });
            }
        });

        if(!headless)
        {
            applicationBuilder.WithExtension<fra::FreyaExtension>([](fra::FreyaExtension &freya) {
                freya.WithOptions([](fra::FreyaOptionsBuilder &freyaOptionsBuilder) {
                    freyaOptionsBuilder.SetTitle("Frigga Application")
                        .SetVSync(true)
                        .SetWidth(1280)
                        .SetHeight(720)
                        .SetAnimationQuality(fra::AnimationQuality::High);
                });
            });
        }
    }

    void FriggaExtension::ConfigureServices(skr::ServiceCollection &services)
    {
        if(mHeadless)
        {
            services.AddSingleton<fg::PrimitiveMeshFactory>(
                [](skr::ServiceProvider &) {
                    return skr::MakeArc<fg::PrimitiveMeshFactory>(fg::PrimitiveMeshFactory::Catalog);
                });
            services.AddSingleton<fg::AssetRegistry>(
                [](skr::ServiceProvider &) {
                    return skr::MakeArc<fg::AssetRegistry>(fg::AssetRegistry::Catalog);
                });
            services.AddSingleton<fg::Scene>([](skr::ServiceProvider &provider) {
                return skr::MakeArc<fg::Scene>(
                    fg::Scene::Headless, provider.GetService<skr::Logger<fg::Scene>>(),
                    provider.GetService<fr::Registry>(),
                    provider.GetService<fg::PrimitiveMeshFactory>(),
                    provider.GetService<fg::AssetRegistry>(),
                    provider.GetService<fg::UserComponentRegistry>());
            });
            services.AddSingleton<fg::Input>([](skr::ServiceProvider &provider) {
                return skr::MakeArc<fg::Input>(skr::Arc<fra::EventManager> {},
                                               provider.GetService<fg::SceneSimulationState>());
            });
        }
        else
        {
            services.AddSingleton<fg::PrimitiveMeshFactory>();
            services.AddSingleton<fg::AssetRegistry>();
            services.AddSingleton<fg::Scene>();
            services.AddSingleton<fg::Input>();
            services.AddScoped<fg::LayerStack>();
            services.AddSingleton<fg::GuiLayer>();
        }

        services.AddSingleton<fg::AnimationController>();
        services.AddSingleton<fg::IPhysicsWorld, fg::JoltPhysicsWorld>();
        services.AddSingleton<fg::Physics>();
        services.AddSingleton<fg::SceneSimulationState>();
        services.AddSingleton<fg::UserComponentRegistry>();
        services.AddSingleton<fg::GameplayPluginHost>();
        services.AddSingleton<fg::Network>();
    }
} // namespace FRIGGA_NAMESPACE
