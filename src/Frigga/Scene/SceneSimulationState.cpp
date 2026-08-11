#include "SceneSimulationState.hpp"

#include "Frigga/ECS/Components/MeshComponent.hpp"
#include "Frigga/ECS/Components/NameComponent.hpp"

namespace FRIGGA_NAMESPACE
{

    SceneSimulationState::SceneSimulationState(
        const skr::Arc<fr::Registry> &registry, const skr::Arc<IPhysicsWorld> &physicsWorld,
        const skr::Arc<Scene> &scene, const skr::Arc<PrimitiveMeshFactory> &primitives,
        const skr::Arc<skr::Logger<SceneSimulationState>> &logger)
        : mRegistry(registry), mPhysicsWorld(physicsWorld), mScene(scene),
          mPrimitives(primitives), mLogger(logger)
    {
    }

    void SceneSimulationState::TogglePlayPause()
    {
        if(!IsPlaying())
        {
            Play();
            return;
        }

        if(mPaused)
        {
            Resume();
        }
        else
        {
            Pause();
        }
    }

    void SceneSimulationState::Play()
    {
        if(IsPlaying())
        {
            Resume();
            return;
        }

        mRegistry->ExecuteTasks();
        snapshotScene();
        buildPhysicsWorld();
        mMode                   = SimulationMode::Play;
        mPaused                 = false;
        mStepRequested          = false;
        mFocusGameplayRequested = true;
        mScene->PreferGameplayCamera();
        mLogger->LogInformation("Entered Play mode");
    }

    void SceneSimulationState::Pause()
    {
        if(!IsPlaying() || mPaused)
        {
            return;
        }

        mPaused        = true;
        mStepRequested = false;
        mLogger->LogInformation("Paused simulation");
    }

    void SceneSimulationState::Resume()
    {
        if(!IsPlaying() || !mPaused)
        {
            return;
        }

        mPaused        = false;
        mStepRequested = false;
        mLogger->LogInformation("Resumed simulation");
    }

    void SceneSimulationState::Stop()
    {
        if(!IsPlaying())
        {
            return;
        }

        teardownPhysicsWorld();
        restoreScene();
        mMode                 = SimulationMode::Edit;
        mPaused               = false;
        mStepRequested        = false;
        mEditSceneSnapshot.clear();
        mFocusEditorRequested = true;
        mScene->PreferEditorCamera();
        mLogger->LogInformation("Exited Play mode");
    }

    void SceneSimulationState::Step()
    {
        if(!IsPlaying())
        {
            return;
        }

        mPaused        = true;
        mStepRequested = true;
    }

    void SceneSimulationState::snapshotScene()
    {
        mEditSceneSnapshot.clear();
        if(!mScene->CaptureSnapshot(mEditSceneSnapshot))
        {
            mLogger->LogError("Failed to snapshot scene before Play; Stop may not restore cleanly");
            mEditSceneSnapshot.clear();
        }
    }

    void SceneSimulationState::restoreScene()
    {
        if(mEditSceneSnapshot.empty())
        {
            mLogger->LogWarning("No edit-mode scene snapshot to restore");
            return;
        }

        if(!mScene->RestoreSnapshot(mEditSceneSnapshot))
        {
            mLogger->LogError("Failed to restore edit-mode scene snapshot");
        }
    }

    PhysicsBodyDesc SceneSimulationState::makeBodyDesc(const TransformComponent &transform,
                                                       const RigidBodyComponent &rigidBody,
                                                       fr::Entity entity) const
    {
        PhysicsBodyDesc desc {
            .motion            = rigidBody.motion,
            .shape             = rigidBody.shape,
            .position          = transform.position,
            .rotation          = transform.rotation,
            .scale             = transform.scale,
            .halfExtents       = rigidBody.halfExtents,
            .radius            = rigidBody.radius,
            .height            = rigidBody.height,
            .mass              = rigidBody.mass,
            .friction          = rigidBody.friction,
            .restitution       = rigidBody.restitution,
            .collisionLayer    = rigidBody.collisionLayer,
            .collideWithLayers = rigidBody.collideWithLayers,
        };

        if(rigidBody.motion == BodyMotionType::Static && desc.collisionLayer == 1)
        {
            desc.collisionLayer = 0;
        }

        if(desc.shape == ColliderShape::Mesh)
        {
            PrimitiveType primitive = PrimitiveType::Cube;
            bool found              = false;
            mRegistry->TryGetComponents<MeshComponent>(entity, [&](MeshComponent &mesh) {
                found = mPrimitives->TryFindPrimitive(mesh.meshId, primitive);
            });
            if(!found)
            {
                primitive = PrimitiveType::Cube;
            }
            desc.meshPoints = PrimitiveMeshFactory::GetColliderHullPoints(primitive);
        }

        return desc;
    }

    void SceneSimulationState::buildPhysicsWorld()
    {
        mPhysicsWorld->Clear();

        mRegistry->CreateMutation()->Each<TransformComponent, RigidBodyComponent>(
            [&](auto entity, TransformComponent &transform, RigidBodyComponent &rigidBody) {
                if(mRegistry->HasComponent<CharacterControllerComponent>(entity))
                {
                    // Character owns locomotion; skip rigid-body creation.
                    rigidBody.body.Reset();
                    return;
                }
                const auto desc = makeBodyDesc(transform, rigidBody, entity);
                rigidBody.body  = mPhysicsWorld->CreateBody(desc);
                if(!rigidBody.body.IsValid())
                {
                    std::string name = "entity";
                    mRegistry->TryGetComponents<NameComponent>(
                        entity, [&](NameComponent &n) { name = n.name; });
                    mLogger->LogWarning("Failed to create physics body for '{}'", name);
                }
            });

        mRegistry->CreateMutation()->Each<TransformComponent, CharacterControllerComponent>(
            [&](auto entity, TransformComponent &transform,
                CharacterControllerComponent &controller) {
                PhysicsCharacterDesc desc {};
                desc.position           = transform.position;
                desc.rotation           = transform.rotation;
                desc.radius             = controller.radius;
                desc.height             = controller.height;
                desc.maxSlopeDegrees    = controller.maxSlopeDegrees;
                desc.mass               = controller.mass;
                desc.centerOffset       = controller.centerOffset;
                desc.collisionLayer     = controller.collisionLayer;
                desc.collideWithLayers  = controller.collideWithLayers;
                controller.character    = mPhysicsWorld->CreateCharacter(desc);
                if(!controller.character.IsValid())
                {
                    std::string name = "entity";
                    mRegistry->TryGetComponents<NameComponent>(
                        entity, [&](NameComponent &n) { name = n.name; });
                    mLogger->LogWarning("Failed to create character controller for '{}'", name);
                }
            });

        mPhysicsWorld->OptimizeBroadPhase();
        mRegistry->ExecuteTasks();
    }

    void SceneSimulationState::teardownPhysicsWorld()
    {
        mRegistry->CreateMutation()->Each<CharacterControllerComponent>(
            [&](auto, CharacterControllerComponent &controller) {
                if(controller.character.IsValid())
                {
                    mPhysicsWorld->DestroyCharacter(controller.character);
                    controller.character.Reset();
                }
            });
        mRegistry->CreateMutation()->Each<RigidBodyComponent>(
            [&](auto, RigidBodyComponent &rigidBody) {
                if(rigidBody.body.IsValid())
                {
                    mPhysicsWorld->DestroyBody(rigidBody.body);
                    rigidBody.body.Reset();
                }
            });
        mPhysicsWorld->Clear();
        mRegistry->ExecuteTasks();
    }

} // namespace FRIGGA_NAMESPACE
