#include <Frigga/Frigga.hpp>

#include <Frigga/Animation/AnimationController.hpp>
#include <Frigga/Animation/AnimationEventRouter.hpp>
#include <Frigga/Asset/AssetRegistry.hpp>
#include <Frigga/Asset/PrimitiveMeshFactory.hpp>
#include <Frigga/Audio/AudioController.hpp>
#include <Frigga/Audio/IAudioEngine.hpp>
#include <Frigga/Audio/MiniaudioEngine.hpp>
#include <Frigga/Core/LayerStack.hpp>
#include <Frigga/ECS/Components/AnimatorComponent.hpp>
#include <Frigga/ECS/Components/AudioSourceComponent.hpp>
#include <Frigga/ECS/Components/BillboardComponent.hpp>
#include <Frigga/ECS/Components/BillboardTextComponent.hpp>
#include <Frigga/ECS/Components/CameraComponent.hpp>
#include <Frigga/ECS/Components/FullscreenEffectComponent.hpp>
#include <Frigga/ECS/Components/HealthBarComponent.hpp>
#include <Frigga/ECS/Components/HierarchyComponent.hpp>
#include <Frigga/ECS/Components/LightComponent.hpp>
#include <Frigga/ECS/Components/MaterialComponent.hpp>
#include <Frigga/ECS/Components/MeshComponent.hpp>
#include <Frigga/ECS/Components/NameComponent.hpp>
#include <Frigga/ECS/Components/ParticleEmitterComponent.hpp>
#include <Frigga/ECS/Components/PrefabComponent.hpp>
#include <Frigga/ECS/Components/RigidBodyComponent.hpp>
#include <Frigga/ECS/Components/TransformComponent.hpp>
#include <Frigga/ECS/Systems/AnimationSystem.hpp>
#include <Frigga/ECS/Systems/AudioSystem.hpp>
#include <Frigga/ECS/Systems/PhysicsSystem.hpp>
#include <Frigga/ECS/Systems/RenderSystem.hpp>
#include <Frigga/ECS/UserComponentRegistry.hpp>
#include <Frigga/Input/Input.hpp>
#include <Frigga/Physics/IPhysicsWorld.hpp>
#include <Frigga/Physics/JoltPhysicsWorld.hpp>
#include <Frigga/Physics/Physics.hpp>
#include <Frigga/Module/GameplayModuleBridge.hpp>
#include <Frigga/Module/GameplayModuleHost.hpp>
#include <Frigga/Scene/Scene.hpp>
#include <Frigga/Scene/SceneSimulationState.hpp>

namespace FRIGGA_NAMESPACE
{
    void FriggaExtension::Attach(skr::ApplicationBuilder &applicationBuilder)
    {
        applicationBuilder
            .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension &freyr) {
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
                    .WithComponent<AudioSourceComponent>()
                    .WithComponent<AudioListenerComponent>()
                    .WithComponent<PrefabComponent>()
                    .WithPipeline([](fr::PipelineBuilder &pipeline) {
                        // Play mode only (Editor disables this pipeline while editing).
                        // Gameplay runs first so fg::Physics intents apply before the step.
                        pipeline.WithName("Simulation")
                            .WithRate(60)
                            .WithSystem<GameplayModuleBridge>()
                            .WithSystem<PhysicsSystem>();
                    })
                    .WithPipeline([](fr::PipelineBuilder &pipeline) {
                        // Play mode only, display rate (e.g. third-person camera, audio).
                        pipeline.WithName("Main").WithSystem<AudioSystem>();
                    })
                    .WithPipeline([](fr::PipelineBuilder &pipeline) {
                        // Always: pose preview then draw. Animation stays here so Edit
                        // can preview clips without ticking Main/Simulation.
                        pipeline.WithName("Render")
                            .WithSystem<AnimationSystem>()
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
        services.AddSingleton<fg::AnimationEventRouter>();
        services.AddSingleton<fg::IAudioEngine, fg::MiniaudioEngine>();
        services.AddSingleton<fg::AudioController>();
        services.AddSingleton<fg::IPhysicsWorld, fg::JoltPhysicsWorld>();
        services.AddSingleton<fg::Physics>();
        services.AddSingleton<fg::Scene>();
        services.AddSingleton<fg::SceneSimulationState>();
        services.AddSingleton<fg::Input>();
        services.AddSingleton<fg::UserComponentRegistry>();
        services.AddSingleton<fg::GameplayModuleHost>();
        services.AddScoped<fg::LayerStack>();
        services.AddSingleton<fg::GuiLayer>();
    }
} // namespace FRIGGA_NAMESPACE
