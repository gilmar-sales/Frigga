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

    void SceneSimulationState::Toggle()
    {
        if(IsPlaying())
        {
            Stop();
        }
        else
        {
            Play();
        }
    }

    void SceneSimulationState::Play()
    {
        if(IsPlaying())
        {
            return;
        }

        mRegistry->ExecuteTasks();
        snapshotTransforms();
        buildPhysicsWorld();
        mMode = SimulationMode::Play;
        mFocusGameplayRequested = true;
        mScene->PreferGameplayCamera();
        mLogger->LogInformation("Entered Play mode");
    }

    void SceneSimulationState::Stop()
    {
        if(!IsPlaying())
        {
            return;
        }

        teardownPhysicsWorld();
        restoreTransforms();
        mEditTransforms.clear();
        mMode = SimulationMode::Edit;
        mFocusEditorRequested = true;
        mScene->PreferEditorCamera();
        mLogger->LogInformation("Exited Play mode");
    }

    void SceneSimulationState::snapshotTransforms()
    {
        mEditTransforms.clear();
        mRegistry->CreateMutation()->Each<TransformComponent>(
            [&](auto entity, TransformComponent &transform) {
                mEditTransforms.emplace(entity, transform);
            });
    }

    void SceneSimulationState::restoreTransforms()
    {
        for(const auto &[entity, transform] : mEditTransforms)
        {
            if(!mRegistry->HasComponent<TransformComponent>(entity))
            {
                continue;
            }
            mRegistry->TryGetComponents<TransformComponent>(
                entity, [&](TransformComponent &current) { current = transform; });
        }
        mRegistry->ExecuteTasks();
    }

    PhysicsBodyDesc SceneSimulationState::makeBodyDesc(const TransformComponent &transform,
                                                       const RigidBodyComponent &rigidBody,
                                                       fr::Entity entity) const
    {
        PhysicsBodyDesc desc {
            .motion             = rigidBody.motion,
            .shape              = rigidBody.shape,
            .position           = transform.position,
            .rotation           = transform.rotation,
            .scale              = transform.scale,
            .halfExtents        = rigidBody.halfExtents,
            .radius             = rigidBody.radius,
            .height             = rigidBody.height,
            .mass               = rigidBody.mass,
            .friction           = rigidBody.friction,
            .restitution        = rigidBody.restitution,
            .collisionLayer     = rigidBody.collisionLayer,
            .collideWithLayers  = rigidBody.collideWithLayers,
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
                const auto desc   = makeBodyDesc(transform, rigidBody, entity);
                rigidBody.body    = mPhysicsWorld->CreateBody(desc);
                if(!rigidBody.body.IsValid())
                {
                    std::string name = "entity";
                    mRegistry->TryGetComponents<NameComponent>(
                        entity, [&](NameComponent &n) { name = n.name; });
                    mLogger->LogWarning("Failed to create physics body for '{}'", name);
                }
            });

        mPhysicsWorld->OptimizeBroadPhase();
        mRegistry->ExecuteTasks();
    }

    void SceneSimulationState::teardownPhysicsWorld()
    {
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
