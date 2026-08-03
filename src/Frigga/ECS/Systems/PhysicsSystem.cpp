#include "PhysicsSystem.hpp"

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

        // Push kinematic transforms authored in the editor into the world.
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

        mPhysicsWorld->Step(deltaTime);

        // Write dynamic/static simulation poses back to ECS transforms.
        mRegistry->CreateMutation()->Each<TransformComponent, RigidBodyComponent>(
            [&](auto, TransformComponent &transform, RigidBodyComponent &rigidBody) {
                if(!rigidBody.body.IsValid() || rigidBody.motion == BodyMotionType::Kinematic)
                {
                    return;
                }
                mPhysicsWorld->GetTransform(rigidBody.body, transform.position, transform.rotation);
            });
    }

} // namespace FRIGGA_NAMESPACE
