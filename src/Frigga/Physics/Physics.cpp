#include "Frigga/Physics/Physics.hpp"

#include "Frigga/ECS/Components/RigidBodyComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"
#include "Frigga/ECS/TransformUtil.hpp"
#include "Frigga/Physics/IPhysicsWorld.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

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

        float CharacterProbeRadius(const RigidBodyComponent &rb)
        {
            if(rb.shape == ColliderShape::Box)
            {
                return std::max({rb.halfExtents.x, rb.halfExtents.z, 0.05f});
            }
            return std::max(rb.radius, 0.05f);
        }

        float CharacterHalfHeight(const RigidBodyComponent &rb)
        {
            switch(rb.shape)
            {
            case ColliderShape::Capsule:
                return std::max(0.5f * rb.height, 0.0f) + std::max(rb.radius, 0.05f);
            case ColliderShape::Sphere:
                return std::max(rb.radius, 0.05f);
            case ColliderShape::Box:
                return std::max(rb.halfExtents.y, 0.05f);
            default:
                return 0.5f;
            }
        }

        CharacterGroundInfo GroundInfoFromBody(const skr::Arc<IPhysicsWorld> &world,
                                               fr::Entity entity, const RigidBodyComponent &rb,
                                               float maxSlopeDegrees)
        {
            CharacterGroundInfo info {};
            if(!world || !rb.body.IsValid())
            {
                return info;
            }

            glm::vec3 position {};
            glm::quat rotation {1.0f, 0.0f, 0.0f, 0.0f};
            world->GetTransform(rb.body, position, rotation);
            info.position = position;
            info.velocity = world->GetLinearVelocity(rb.body);

            const float halfHeight = CharacterHalfHeight(rb);
            const float radius     = CharacterProbeRadius(rb);
            const float skin       = 0.08f;
            const glm::vec3 origin = position + glm::vec3 {0.0f, 0.05f, 0.0f};
            const float maxDist    = halfHeight + skin + 0.05f;

            QueryFilter filter {};
            filter.ignoreEntity = static_cast<std::uint64_t>(entity);

            RaycastHit hit = world->SphereCast(origin, {0.0f, -1.0f, 0.0f}, radius * 0.85f,
                                               maxDist, filter);
            if(!hit.hit)
            {
                hit = world->Raycast(origin, {0.0f, -1.0f, 0.0f}, maxDist, filter);
            }
            if(!hit.hit)
            {
                info.state = CharacterGroundState::InAir;
                return info;
            }

            info.normal     = hit.normal;
            info.groundBody = hit.body;
            const float maxSlopeRad =
                glm::radians(std::clamp(maxSlopeDegrees, 1.0f, 89.0f));
            const float minY = std::cos(maxSlopeRad);
            if(hit.normal.y >= minY)
            {
                info.grounded = true;
                info.state     = CharacterGroundState::OnGround;
            }
            else
            {
                info.grounded = false;
                info.state     = CharacterGroundState::OnSteepGround;
            }
            return info;
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

        TransformUtil::SetWorldPose(*mRegistry, entity, position, rotation);

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

    void Physics::SetAngularVelocity(fr::Entity entity, const glm::vec3 &velocity)
    {
        if(!mWorld)
        {
            return;
        }
        const auto handle = BodyHandle(mRegistry, entity);
        if(handle.IsValid())
        {
            mWorld->SetAngularVelocity(handle, velocity);
        }
    }

    glm::vec3 Physics::GetAngularVelocity(fr::Entity entity) const
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
        return mWorld->GetAngularVelocity(handle);
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

    void Physics::AddTorque(fr::Entity entity, const glm::vec3 &torque)
    {
        if(!mWorld)
        {
            return;
        }
        const auto handle = BodyHandle(mRegistry, entity);
        if(handle.IsValid())
        {
            mWorld->AddTorque(handle, torque);
        }
    }

    void Physics::AddAngularImpulse(fr::Entity entity, const glm::vec3 &impulse)
    {
        if(!mWorld)
        {
            return;
        }
        const auto handle = BodyHandle(mRegistry, entity);
        if(handle.IsValid())
        {
            mWorld->AddAngularImpulse(handle, impulse);
        }
    }

    void Physics::MoveCharacter(fr::Entity entity, const glm::vec3 &desiredWorldVelocity)
    {
        SetLinearVelocity(entity, desiredWorldVelocity);
    }

    void Physics::TeleportCharacter(fr::Entity entity, const glm::vec3 &worldPosition)
    {
        if(!mRegistry || !mWorld)
        {
            return;
        }

        glm::quat rotation {1.0f, 0.0f, 0.0f, 0.0f};
        if(mRegistry->HasComponent<TransformComponent>(entity))
        {
            rotation = TransformUtil::WorldPose(*mRegistry, entity).rotation;
            TransformUtil::SetWorldPosition(*mRegistry, entity, worldPosition);
        }

        const auto handle = BodyHandle(mRegistry, entity);
        if(handle.IsValid())
        {
            mWorld->SetTransform(handle, worldPosition, rotation);
            mWorld->SetLinearVelocity(handle, {});
            mWorld->SetAngularVelocity(handle, {});
        }
    }

    void Physics::SetCharacterFacing(fr::Entity entity, const glm::quat &worldRotation)
    {
        if(!mRegistry || !mWorld)
        {
            return;
        }

        glm::vec3 position {};
        if(mRegistry->HasComponent<TransformComponent>(entity))
        {
            const auto pose = TransformUtil::WorldPose(*mRegistry, entity);
            position        = pose.position;
            TransformUtil::SetWorldPose(*mRegistry, entity, pose.position, worldRotation);
        }

        const auto handle = BodyHandle(mRegistry, entity);
        if(handle.IsValid())
        {
            if(position == glm::vec3 {})
            {
                glm::quat ignored {1.0f, 0.0f, 0.0f, 0.0f};
                mWorld->GetTransform(handle, position, ignored);
            }
            mWorld->SetTransform(handle, position, worldRotation);
            mWorld->SetAngularVelocity(handle, {});
        }
    }

    bool Physics::IsCharacterGrounded(fr::Entity entity) const
    {
        return GetCharacterGroundInfo(entity).grounded;
    }

    glm::vec3 Physics::GetCharacterVelocity(fr::Entity entity) const
    {
        return GetLinearVelocity(entity);
    }

    CharacterGroundInfo Physics::GetCharacterGroundInfo(fr::Entity entity) const
    {
        if(!mRegistry || !mWorld)
        {
            return {};
        }

        CharacterGroundInfo info {};
        mRegistry->TryGetComponents<RigidBodyComponent>(entity, [&](RigidBodyComponent &rb) {
            info = GroundInfoFromBody(mWorld, entity, rb, 45.0f);
        });
        return info;
    }

    bool Physics::SetCharacterShape(fr::Entity entity, float radius, float height,
                                    const glm::vec3 &centerOffset)
    {
        if(!mRegistry)
        {
            return false;
        }
        bool updated = false;
        mRegistry->TryGetComponents<RigidBodyComponent>(entity, [&](RigidBodyComponent &rb) {
            rb.shape        = ColliderShape::Capsule;
            rb.radius       = std::max(radius, 0.001f);
            rb.height       = std::max(height, 0.0f);
            rb.centerOffset = centerOffset;
            updated         = true;
        });
        // Runtime shape swap on an existing body is not supported yet — values apply next Play.
        return updated;
    }

    bool Physics::EnsureBody(fr::Entity entity, bool lockRotation)
    {
        if(!mRegistry || !mWorld || !mRegistry->HasComponent<RigidBodyComponent>(entity))
        {
            return false;
        }

        bool ok = false;
        mRegistry->TryGetComponents<RigidBodyComponent>(entity, [&](RigidBodyComponent &rb) {
            if(rb.body.IsValid())
            {
                ok = true;
                return;
            }
            if(!mRegistry->HasComponent<TransformComponent>(entity))
            {
                return;
            }

            const auto pose = TransformUtil::WorldPose(*mRegistry, entity);
            PhysicsBodyDesc desc {};
            desc.motion            = rb.motion;
            desc.shape             = rb.shape;
            desc.position          = pose.position;
            desc.rotation          = pose.rotation;
            desc.scale             = pose.scale;
            desc.halfExtents       = rb.halfExtents;
            desc.radius            = rb.radius;
            desc.height            = rb.height;
            desc.centerOffset      = rb.centerOffset;
            desc.mass              = rb.mass;
            desc.friction          = rb.friction;
            desc.restitution       = rb.restitution;
            desc.collisionLayer    = rb.collisionLayer;
            desc.collideWithLayers = rb.collideWithLayers;
            desc.isSensor          = rb.isSensor;
            desc.entityId          = static_cast<std::uint64_t>(entity);
            if(lockRotation)
            {
                desc.lockRotationX = true;
                desc.lockRotationY = true;
                desc.lockRotationZ = true;
            }
            if(desc.motion != BodyMotionType::Static && desc.collisionLayer == 0)
            {
                desc.collisionLayer = 1;
            }
            rb.body = mWorld->CreateBody(desc);
            ok      = rb.body.IsValid();
        });
        return ok;
    }

    void Physics::DestroyBody(fr::Entity entity)
    {
        if(!mRegistry || !mWorld)
        {
            return;
        }
        mRegistry->TryGetComponents<RigidBodyComponent>(entity, [&](RigidBodyComponent &rb) {
            if(rb.body.IsValid())
            {
                mWorld->DestroyBody(rb.body);
                rb.body.Reset();
            }
        });
    }

    PhysicsJointHandle Physics::CreateJoint(fr::Entity entityA, fr::Entity entityB,
                                            PhysicsJointType type, const glm::vec3 &localAnchorA,
                                            const glm::vec3 &localAnchorB,
                                            const glm::vec3 &hingeAxisLocalA)
    {
        if(!mWorld)
        {
            return {};
        }
        const auto bodyA = BodyHandle(mRegistry, entityA);
        const auto bodyB = BodyHandle(mRegistry, entityB);
        if(!bodyA.IsValid() || !bodyB.IsValid())
        {
            return {};
        }
        PhysicsJointDesc desc {};
        desc.type            = type;
        desc.bodyA           = bodyA;
        desc.bodyB           = bodyB;
        desc.localAnchorA    = localAnchorA;
        desc.localAnchorB    = localAnchorB;
        desc.hingeAxisLocalA = hingeAxisLocalA;
        return mWorld->CreateJoint(desc);
    }

    void Physics::DestroyJoint(PhysicsJointHandle handle)
    {
        if(mWorld)
        {
            mWorld->DestroyJoint(handle);
        }
    }

    RaycastHit Physics::Raycast(const glm::vec3 &origin, const glm::vec3 &direction,
                                float maxDistance, const QueryFilter &filter) const
    {
        if(!mWorld)
        {
            return {};
        }
        return mWorld->Raycast(origin, direction, maxDistance, filter);
    }

    RaycastHit Physics::SphereCast(const glm::vec3 &origin, const glm::vec3 &direction,
                                   float radius, float maxDistance,
                                   const QueryFilter &filter) const
    {
        if(!mWorld)
        {
            return {};
        }
        return mWorld->SphereCast(origin, direction, radius, maxDistance, filter);
    }

    RaycastHit Physics::CapsuleCast(const glm::vec3 &origin, const glm::vec3 &direction,
                                    float radius, float height, float maxDistance,
                                    const QueryFilter &filter) const
    {
        if(!mWorld)
        {
            return {};
        }
        return mWorld->CapsuleCast(origin, direction, radius, height, maxDistance, filter);
    }

    std::vector<OverlapHit> Physics::OverlapSphere(const glm::vec3 &center, float radius,
                                                   const QueryFilter &filter) const
    {
        if(!mWorld)
        {
            return {};
        }
        return mWorld->OverlapSphere(center, radius, filter);
    }

    std::vector<OverlapHit> Physics::OverlapBox(const glm::vec3 &center,
                                                const glm::vec3 &halfExtents,
                                                const glm::quat &rotation,
                                                const QueryFilter &filter) const
    {
        if(!mWorld)
        {
            return {};
        }
        return mWorld->OverlapBox(center, halfExtents, rotation, filter);
    }

    std::vector<TriggerEvent> Physics::DrainTriggerEvents()
    {
        if(!mWorld)
        {
            return {};
        }
        return mWorld->DrainTriggerEvents();
    }

    std::vector<PhysicsContactEvent> Physics::DrainContactEvents()
    {
        if(!mWorld)
        {
            return {};
        }
        return mWorld->DrainContactEvents();
    }

    float Physics::GetInterpolationAlpha() const
    {
        if(!mWorld)
        {
            return 0.0f;
        }
        return mWorld->GetInterpolationAlpha();
    }

} // namespace FRIGGA_NAMESPACE
