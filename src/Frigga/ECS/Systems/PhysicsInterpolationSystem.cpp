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

        mPhysicsWorld->ForEachCharacter(
            [&](std::uint64_t rawEntity, PhysicsCharacterHandle character) {
                const auto entity = static_cast<fr::Entity>(rawEntity);
                if(!character.IsValid() || !mRegistry->HasComponent<TransformComponent>(entity))
                {
                    return;
                }
                glm::vec3 position {};
                if(!mPhysicsWorld->GetInterpolatedCharacterPosition(character, alpha, position))
                {
                    return;
                }
                TransformUtil::SetWorldPosition(*mRegistry, entity, position);
            });
    }

} // namespace FRIGGA_NAMESPACE
