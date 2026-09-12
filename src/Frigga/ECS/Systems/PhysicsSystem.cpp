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

        mRegistry->CreateMutation()
            ->WithLabel("Push kinematic transforms")
            .EachAsync([this](fr::Entity entity, const TransformComponent &,
                              const RigidBodyComponent &rigidBody) {
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
            FREYR_TRACE("APP", "PhysicsWorld::StepFixed");
            mPhysicsWorld->StepFixed(1);
        }
        else
        {
            FREYR_TRACE("APP", "PhysicsWorld::Step");
            mPhysicsWorld->Step(deltaTime);
        }

        mRegistry->CreateMutation()
            ->WithLabel("Write dynamic simulation poses back")
            .EachAsync(
                [this](fr::Entity entity, TransformComponent &, RigidBodyComponent &rigidBody) {
                    if(!rigidBody.body.IsValid() || rigidBody.motion == BodyMotionType::Kinematic)
                    {
                        return;
                    }
                    glm::vec3 position{};
                    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
                    mPhysicsWorld->GetTransform(rigidBody.body, position, rotation);
                    TransformUtil::SetWorldPose(*mRegistry, entity, position, rotation);
                });
        mRegistry->ExecuteTasks();
    }

} // namespace FRIGGA_NAMESPACE
