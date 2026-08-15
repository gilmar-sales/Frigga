#include "PhysicsSystem.hpp"

#include "Frigga/ECS/Components/CharacterControllerComponent.hpp"
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
        mRegistry->CreateMutation()->Each<TransformComponent, RigidBodyComponent>(
            [&](auto entity, TransformComponent &, RigidBodyComponent &rigidBody) {
                if(!rigidBody.body.IsValid())
                {
                    return;
                }
                if(rigidBody.motion == BodyMotionType::Kinematic)
                {
                    const auto pose = TransformUtil::WorldPose(*mRegistry, entity);
                    mPhysicsWorld->SetTransform(rigidBody.body, pose.position, pose.rotation);
                }
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
        mRegistry->CreateMutation()->Each<TransformComponent, RigidBodyComponent>(
            [&](auto entity, TransformComponent &, RigidBodyComponent &rigidBody) {
                if(mRegistry->HasComponent<CharacterControllerComponent>(entity))
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

        // CharacterVirtual owns pose for entities with a character controller.
        mRegistry->CreateMutation()->Each<TransformComponent, CharacterControllerComponent>(
            [&](auto entity, TransformComponent &, CharacterControllerComponent &character) {
                if(!character.character.IsValid())
                {
                    return;
                }
                glm::vec3 position {};
                glm::quat rotation {1.0f, 0.0f, 0.0f, 0.0f};
                mPhysicsWorld->GetCharacterTransform(character.character, position, rotation);
                TransformUtil::SetWorldPose(*mRegistry, entity, position, rotation);
            });
    }

} // namespace FRIGGA_NAMESPACE
