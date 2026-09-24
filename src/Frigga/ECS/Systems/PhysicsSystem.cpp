#include <Frigga/ECS/Systems/PhysicsSystem.hpp>

#include "Frigga/ECS/Components/RigidBodyComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"
#include "Frigga/ECS/TransformUtil.hpp"

#include <vector>

namespace FRIGGA_NAMESPACE
{

    void WriteBodyPose(fr::Registry &registry, fr::Entity entity, TransformComponent &transform,
                       const glm::vec3 &position, const glm::quat &rotation)
    {
        const glm::mat4 parentWorld = TransformUtil::ParentWorldMatrix(registry, entity);
        const glm::vec3 worldScale =
            TransformUtil::Decompose(parentWorld * TransformUtil::LocalMatrix(transform)).scale;
        glm::mat4 world = glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation);
        world           = glm::scale(world, worldScale);
        TransformUtil::ApplyLocalMatrix(transform,
                                        TransformUtil::LocalFromWorld(parentWorld, world));
    }

    void MarkDynamicBodiesDirty(fr::Registry &registry)
    {
        std::vector<fr::Entity> dirty;
        registry.CreateMutation()->Each(
            [&](fr::Entity entity, TransformComponent &, RigidBodyComponent &rigidBody) {
                if(rigidBody.body.IsValid() && rigidBody.motion != BodyMotionType::Kinematic)
                {
                    dirty.push_back(entity);
                }
            });
        for(const auto entity : dirty)
        {
            TransformUtil::MarkDirty(registry, entity);
        }
    }

    PhysicsSystem::PhysicsSystem(const skr::Arc<fr::Registry> &registry,
                                 const skr::Arc<IPhysicsWorld> &physicsWorld,
                                 const skr::Arc<SceneSimulationState> &simulation)
        : System(registry), mPhysicsWorld(physicsWorld), mSimulation(simulation)
    {
    }

    void PhysicsSystem::PreUpdate(float deltaTime)
    {
        if(!mSimulation->IsPlaying())
        {
            return;
        }

        mStepOnce = mSimulation->ConsumeStepRequest();
        if(!mSimulation->IsRunning() && !mStepOnce)
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
                const auto pose = TransformUtil::GetWorldPose(*mRegistry, entity);
                mPhysicsWorld->SetTransform(rigidBody.body, pose.position, pose.rotation);
            });
    }

    void PhysicsSystem::Update(float deltaTime)
    {
        if(mStepOnce)
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
            .EachAsync([this](fr::Entity entity, TransformComponent &transform,
                              RigidBodyComponent &rigidBody) {
                if(!rigidBody.body.IsValid() || rigidBody.motion == BodyMotionType::Kinematic)
                {
                    return;
                }
                glm::vec3 position{};
                glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
                mPhysicsWorld->GetTransform(rigidBody.body, position, rotation);
                WriteBodyPose(*mRegistry, entity, transform, position, rotation);
            });
        MarkDynamicBodiesDirty(*mRegistry);
    }

} // namespace FRIGGA_NAMESPACE
