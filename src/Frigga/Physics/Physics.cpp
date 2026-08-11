#include "Frigga/Physics/Physics.hpp"

#include "Frigga/ECS/Components/CharacterControllerComponent.hpp"
#include "Frigga/ECS/Components/RigidBodyComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"
#include "Frigga/Physics/IPhysicsWorld.hpp"

namespace FRIGGA_NAMESPACE
{

    namespace
    {
        PhysicsBodyHandle BodyHandle(const skr::Arc<fr::Registry> &registry, fr::Entity entity)
        {
            PhysicsBodyHandle handle {};
            if(!registry)
            {
                return handle;
            }
            registry->TryGetComponents<RigidBodyComponent>(entity, [&](RigidBodyComponent &rb) {
                handle = rb.body;
            });
            return handle;
        }

        PhysicsCharacterHandle CharacterHandle(const skr::Arc<fr::Registry> &registry,
                                               fr::Entity entity)
        {
            PhysicsCharacterHandle handle {};
            if(!registry)
            {
                return handle;
            }
            registry->TryGetComponents<CharacterControllerComponent>(
                entity, [&](CharacterControllerComponent &cc) { handle = cc.character; });
            return handle;
        }
    } // namespace

    Physics::Physics(const skr::Arc<fr::Registry> &registry, const skr::Arc<IPhysicsWorld> &world)
        : mRegistry(registry), mWorld(world)
    {
    }

    Physics::~Physics() = default;

    void Physics::SetKinematicPose(fr::Entity entity, const glm::vec3 &position,
                                   const glm::quat &rotation)
    {
        if(!mRegistry || !mWorld)
        {
            return;
        }

        mRegistry->TryGetComponents<TransformComponent>(entity, [&](TransformComponent &transform) {
            transform.position = position;
            transform.rotation = rotation;
        });

        const auto handle = BodyHandle(mRegistry, entity);
        if(handle.IsValid())
        {
            mWorld->SetTransform(handle, position, rotation);
        }
    }

    void Physics::SetLinearVelocity(fr::Entity entity, const glm::vec3 &velocity)
    {
        if(!mWorld)
        {
            return;
        }
        const auto handle = BodyHandle(mRegistry, entity);
        if(handle.IsValid())
        {
            mWorld->SetLinearVelocity(handle, velocity);
        }
    }

    glm::vec3 Physics::GetLinearVelocity(fr::Entity entity) const
    {
        if(!mWorld)
        {
            return {};
        }
        const auto handle = BodyHandle(mRegistry, entity);
        if(!handle.IsValid())
        {
            return {};
        }
        return mWorld->GetLinearVelocity(handle);
    }

    void Physics::AddImpulse(fr::Entity entity, const glm::vec3 &impulse)
    {
        if(!mWorld)
        {
            return;
        }
        const auto handle = BodyHandle(mRegistry, entity);
        if(handle.IsValid())
        {
            mWorld->AddImpulse(handle, impulse);
        }
    }

    void Physics::AddForce(fr::Entity entity, const glm::vec3 &force)
    {
        if(!mWorld)
        {
            return;
        }
        const auto handle = BodyHandle(mRegistry, entity);
        if(handle.IsValid())
        {
            mWorld->AddForce(handle, force);
        }
    }

    void Physics::MoveCharacter(fr::Entity entity, const glm::vec3 &desiredWorldVelocity)
    {
        if(!mWorld)
        {
            return;
        }
        const auto handle = CharacterHandle(mRegistry, entity);
        if(handle.IsValid())
        {
            mWorld->SetCharacterVelocity(handle, desiredWorldVelocity);
        }
    }

    bool Physics::IsCharacterGrounded(fr::Entity entity) const
    {
        if(!mWorld)
        {
            return false;
        }
        const auto handle = CharacterHandle(mRegistry, entity);
        if(!handle.IsValid())
        {
            return false;
        }
        return mWorld->IsCharacterGrounded(handle);
    }

    glm::vec3 Physics::GetCharacterVelocity(fr::Entity entity) const
    {
        if(!mWorld)
        {
            return {};
        }
        const auto handle = CharacterHandle(mRegistry, entity);
        if(!handle.IsValid())
        {
            return {};
        }
        return mWorld->GetCharacterVelocity(handle);
    }

} // namespace FRIGGA_NAMESPACE
