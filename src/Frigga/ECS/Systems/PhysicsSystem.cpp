#include "PhysicsSystem.hpp"

#include "Frigga/ECS/Components/CharacterControllerComponent.hpp"
#include "Frigga/ECS/Components/RigidBodyComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"

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
            [&](auto, TransformComponent &transform, RigidBodyComponent &rigidBody) {
                if(!rigidBody.body.IsValid())
                {
                    return;
                }
                if(rigidBody.motion == BodyMotionType::Kinematic)
                {
                    mPhysicsWorld->SetTransform(rigidBody.body, transform.position,
                                                transform.rotation);
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
            [&](auto entity, TransformComponent &transform, RigidBodyComponent &rigidBody) {
                if(mRegistry->HasComponent<CharacterControllerComponent>(entity))
                {
                    return;
                }
                if(!rigidBody.body.IsValid() || rigidBody.motion == BodyMotionType::Kinematic)
                {
                    return;
                }
                mPhysicsWorld->GetTransform(rigidBody.body, transform.position, transform.rotation);
            });

        // CharacterVirtual owns pose for entities with a character controller.
        mRegistry->CreateMutation()->Each<TransformComponent, CharacterControllerComponent>(
            [&](auto, TransformComponent &transform, CharacterControllerComponent &character) {
                if(!character.character.IsValid())
                {
                    return;
                }
                mPhysicsWorld->GetCharacterTransform(character.character, transform.position,
                                                     transform.rotation);
            });
    }

} // namespace FRIGGA_NAMESPACE
