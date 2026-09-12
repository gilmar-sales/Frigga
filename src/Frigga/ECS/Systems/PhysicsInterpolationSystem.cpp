#include <Frigga/ECS/Systems/PhysicsInterpolationSystem.hpp>

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

    void PhysicsInterpolationSystem::Update(float)
    {
        if(!mSimulation->IsPlaying() || !mSimulation->IsRunning())
        {
            return;
        }

        const float alpha = mPhysicsWorld->GetInterpolationAlpha();

        mRegistry->CreateMutation()->EachAsync(
            [&](fr::Entity entity, TransformComponent &, RigidBodyComponent &rigidBody) {
                if(!rigidBody.body.IsValid() || rigidBody.motion == BodyMotionType::Kinematic)
                {
                    return;
                }
                glm::vec3 position {};
                glm::quat rotation {1.0f, 0.0f, 0.0f, 0.0f};
                if(!mPhysicsWorld->GetInterpolatedBodyPose(rigidBody.body, alpha, position,
                                                           rotation))
                {
                    return;
                }
                TransformUtil::SetWorldPose(*mRegistry, entity, position, rotation);
            });
    }

} // namespace FRIGGA_NAMESPACE
