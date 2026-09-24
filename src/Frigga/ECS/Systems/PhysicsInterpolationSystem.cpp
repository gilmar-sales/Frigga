#include <Frigga/ECS/Systems/PhysicsInterpolationSystem.hpp>
#include <Frigga/ECS/Systems/PhysicsSystem.hpp>

#include "Frigga/ECS/Components/RigidBodyComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"
#include "Frigga/ECS/TransformUtil.hpp"

namespace FRIGGA_NAMESPACE
{

    PhysicsInterpolationSystem::PhysicsInterpolationSystem(
        const skr::Arc<fr::Registry> &registry, const skr::Arc<IPhysicsWorld> &physicsWorld,
        const skr::Arc<SceneSimulationState> &simulation)
        : System(registry), mPhysicsWorld(physicsWorld), mSimulation(simulation)
    {
    }

    void PhysicsInterpolationSystem::PostUpdate(float)
    {
        if(!mSimulation->IsPlaying() || !mSimulation->IsRunning())
        {
            return;
        }

        mRegistry->CreateMutation()->EachAsync(
            [this, alpha = mPhysicsWorld->GetInterpolationAlpha()](
                fr::Entity entity, TransformComponent &transform, RigidBodyComponent &rigidBody) {
                if(!rigidBody.body.IsValid() || rigidBody.motion == BodyMotionType::Kinematic)
                {
                    return;
                }
                glm::vec3 position{};
                glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
                if(!mPhysicsWorld->GetInterpolatedBodyPose(rigidBody.body, alpha, position,
                                                           rotation))
                {
                    return;
                }
                WriteBodyPose(*mRegistry, entity, transform, position, rotation);
            });
        MarkDynamicBodiesDirty(*mRegistry);
    }

} // namespace FRIGGA_NAMESPACE
