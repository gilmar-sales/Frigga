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
        mRegistry->CreateMutation()->EachAsync(
            [this](fr::Entity entity, const TransformComponent &, const RigidBodyComponent &rigidBody) {
                if(!rigidBody.body.IsValid() || rigidBody.motion != BodyMotionType::Kinematic)
                {
                    return;
                }
                const auto pose = TransformUtil::WorldPose(*mRegistry, entity);
                mPhysicsWorld->SetTransform(rigidBody.body, pose.position, pose.rotation);
            });

            mRegistry->ExecuteTasks();
        if(stepOnce)
        {
            mPhysicsWorld->StepFixed(1);
        }
        else
        {
            mPhysicsWorld->Step(deltaTime);
        }

        // Write dynamic simulation poses back to ECS transforms.
        mRegistry->CreateMutation()->EachAsync(
            [this](fr::Entity entity, TransformComponent &, RigidBodyComponent &rigidBody) {
                if(!rigidBody.body.IsValid() || rigidBody.motion == BodyMotionType::Kinematic)
                {
                    return;
                }
                glm::vec3 position {};
                glm::quat rotation {1.0f, 0.0f, 0.0f, 0.0f};
                mPhysicsWorld->GetTransform(rigidBody.body, position, rotation);
                TransformUtil::SetWorldPose(*mRegistry, entity, position, rotation);
            });
            mRegistry->ExecuteTasks();
    }

} // namespace FRIGGA_NAMESPACE
