#include "SceneSimulationState.hpp"

#include "Frigga/ECS/Components/MeshComponent.hpp"
#include "Frigga/ECS/Components/NameComponent.hpp"
#include "Frigga/ECS/TransformUtil.hpp"
#include "Frigga/Physics/CharacterPhysics.hpp"

#include <cstdint>

namespace FRIGGA_NAMESPACE
{

    SceneSimulationState::SceneSimulationState(
        const skr::Arc<fr::Registry> &registry, const skr::Arc<IPhysicsWorld> &physicsWorld,
        const skr::Arc<Scene> &scene, const skr::Arc<PrimitiveMeshFactory> &primitives,
        const skr::Arc<UserComponentRegistry> &userComponents,
        const skr::Arc<skr::Logger<SceneSimulationState>> &logger)
        : mRegistry(registry), mPhysicsWorld(physicsWorld), mScene(scene),
          mPrimitives(primitives), mUserComponents(userComponents), mLogger(logger)
    {
    }

    void SceneSimulationState::queue(PendingCommand command)
    {
        if(mDeferModeChanges)
        {
            mPending = command;
            return;
        }
        mPending = PendingCommand::None;
        switch(command)
        {
        case PendingCommand::Play:
            applyPlay();
            break;
        case PendingCommand::Stop:
            applyStop();
            break;
        case PendingCommand::Pause:
            applyPause();
            break;
        case PendingCommand::Resume:
            applyResume();
            break;
        case PendingCommand::Step:
            applyStep();
            break;
        case PendingCommand::None:
            break;
        }
    }

    void SceneSimulationState::FlushPending()
    {
        const auto pending = mPending;
        mPending           = PendingCommand::None;
        switch(pending)
        {
        case PendingCommand::Play:
            applyPlay();
            break;
        case PendingCommand::Stop:
            applyStop();
            break;
        case PendingCommand::Pause:
            applyPause();
            break;
        case PendingCommand::Resume:
            applyResume();
            break;
        case PendingCommand::Step:
            applyStep();
            break;
        case PendingCommand::None:
            break;
        }
    }

    void SceneSimulationState::TogglePlayPause()
    {
        if(!IsPlaying())
        {
            queue(PendingCommand::Play);
            return;
        }

        queue(mPaused ? PendingCommand::Resume : PendingCommand::Pause);
    }

    void SceneSimulationState::Play()
    {
        queue(PendingCommand::Play);
    }

    void SceneSimulationState::applyPlay()
    {
        if(IsPlaying())
        {
            applyResume();
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
        queue(PendingCommand::Pause);
    }

    void SceneSimulationState::applyPause()
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
        queue(PendingCommand::Resume);
    }

    void SceneSimulationState::applyResume()
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
        queue(PendingCommand::Stop);
    }

    void SceneSimulationState::applyStop()
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
        queue(PendingCommand::Step);
    }

    void SceneSimulationState::applyStep()
    {
        if(!IsPlaying())
        {
            return;
        }

        mPaused        = true;
        mStepRequested = true;
    }

    PhysicsCharacterHandle SceneSimulationState::CharacterHandleOf(fr::Entity entity) const
    {
        if(!mPhysicsWorld)
        {
            return {};
        }
        return mPhysicsWorld->FindCharacter(static_cast<std::uint64_t>(entity));
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
        (void)transform;
        const auto pose = TransformUtil::WorldPose(*mRegistry, entity);
        PhysicsBodyDesc desc {
            .motion            = rigidBody.motion,
            .shape             = rigidBody.shape,
            .position          = pose.position,
            .rotation          = pose.rotation,
            .scale             = pose.scale,
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
                if(mUserComponents &&
                   EntityHasCharacterController(*mRegistry, *mUserComponents, entity))
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

        if(mUserComponents)
        {
            const auto ops = mUserComponents->Find(kCharacterControllerTypeId);
            if(ops && ops->forEachEntity && ops->toInstance)
            {
                ops->forEachEntity(*mRegistry, [&](fr::Entity entity) {
                    if(!mRegistry->HasComponent<TransformComponent>(entity))
                    {
                        return;
                    }
                    UserComponentInstance instance {};
                    if(!ops->toInstance(*mRegistry, entity, instance))
                    {
                        return;
                    }
                    const auto pose = TransformUtil::WorldPose(*mRegistry, entity);
                    auto desc       = CharacterDescFromInstance(instance);
                    desc.position   = pose.position;
                    desc.rotation   = pose.rotation;
                    const auto handle = mPhysicsWorld->CreateCharacter(desc);
                    mPhysicsWorld->BindCharacter(static_cast<std::uint64_t>(entity), handle);
                    if(!handle.IsValid())
                    {
                        std::string name = "entity";
                        mRegistry->TryGetComponents<NameComponent>(
                            entity, [&](NameComponent &n) { name = n.name; });
                        mLogger->LogWarning("Failed to create character controller for '{}'", name);
                    }
                });
            }
        }

        mPhysicsWorld->OptimizeBroadPhase();
        mRegistry->ExecuteTasks();
    }

    void SceneSimulationState::teardownPhysicsWorld()
    {
        mPhysicsWorld->ForEachCharacter(
            [&](std::uint64_t, PhysicsCharacterHandle handle) {
                if(handle.IsValid())
                {
                    mPhysicsWorld->DestroyCharacter(handle);
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
