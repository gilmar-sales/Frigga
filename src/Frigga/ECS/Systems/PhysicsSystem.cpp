#include <Frigga/ECS/Systems/PhysicsSystem.hpp>

#include "Frigga/ECS/Components/RigidBodyComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"
#include "Frigga/ECS/TransformUtil.hpp"

namespace FRIGGA_NAMESPACE
{

    PhysicsSystem::PhysicsSystem(const skr::Arc<fr::Registry> &registry,
                                 const skr::Arc<IPhysicsWorld> &physicsWorld,
                                 const skr::Arc<SceneSimulationState> &simulation)
        : System(registry), mPhysicsWorld(physicsWorld), mSimulation(simulation)
    {
    }

    void PhysicsSystem::Update(float deltaTime)
    {
        if(!mSimulation->IsPlaying())
        {
            return;
        }

        const bool stepOnce = mSimulation->ConsumeStepRequest();
        if(!mSimulation->IsRunning() && !stepOnce)
        {
            return;
        }

        // Push kinematic transforms authored by gameplay / editor into the world.
        // CharacterController entities own their linked RigidBody via CharacterVirtual sync —
        // do not overwrite that presence body from Transform here (would fight / look like a 2nd body).
        mRegistry->CreateMutation()->Each(
            [&](fr::Entity entity, TransformComponent &, RigidBodyComponent &rigidBody) {
                if(!rigidBody.body.IsValid())
                {
                    return;
                }
                if(mPhysicsWorld->FindCharacter(static_cast<std::uint64_t>(entity)).IsValid())
                {
                    return;
                }
                if(rigidBody.motion == BodyMotionType::Kinematic)
                {
                    const auto pose = TransformUtil::WorldPose(*mRegistry, entity);
                    mPhysicsWorld->SetTransform(rigidBody.body, pose.position, pose.rotation);
                }
            });

        // Gameplay owns character facing: push Transform rotation into CharacterVirtual.
        mPhysicsWorld->ForEachCharacter(
            [&](std::uint64_t rawEntity, PhysicsCharacterHandle character) {
                const auto entity = static_cast<fr::Entity>(rawEntity);
                if(!character.IsValid() || !mRegistry->HasComponent<TransformComponent>(entity))
                {
                    return;
                }
                const auto pose = TransformUtil::WorldPose(*mRegistry, entity);
                mPhysicsWorld->SetCharacterRotation(character, pose.rotation);
            });

        if(stepOnce)
        {
            mPhysicsWorld->StepFixed(1);
        }
        else
        {
            mPhysicsWorld->Step(deltaTime);
        }

        // Write dynamic simulation poses back to ECS transforms.
        mRegistry->CreateMutation()->Each(
            [&](fr::Entity entity, TransformComponent &, RigidBodyComponent &rigidBody) {
                if(mPhysicsWorld->FindCharacter(static_cast<std::uint64_t>(entity)).IsValid())
                {
                    return;
                }
                if(!rigidBody.body.IsValid() || rigidBody.motion == BodyMotionType::Kinematic)
                {
                    return;
                }
                glm::vec3 position {};
                glm::quat rotation {1.0f, 0.0f, 0.0f, 0.0f};
                mPhysicsWorld->GetTransform(rigidBody.body, position, rotation);
                TransformUtil::SetWorldPose(*mRegistry, entity, position, rotation);
            });

        // Physics owns character position only; preserve ECS facing/yaw.
        mPhysicsWorld->ForEachCharacter(
            [&](std::uint64_t rawEntity, PhysicsCharacterHandle character) {
                const auto entity = static_cast<fr::Entity>(rawEntity);
                if(!character.IsValid() || !mRegistry->HasComponent<TransformComponent>(entity))
                {
                    return;
                }
                glm::vec3 position {};
                glm::quat rotation {1.0f, 0.0f, 0.0f, 0.0f};
                mPhysicsWorld->GetCharacterTransform(character, position, rotation);
                TransformUtil::SetWorldPosition(*mRegistry, entity, position);
            });
    }

} // namespace FRIGGA_NAMESPACE
