#pragma once

#include "Frigga/Macro.hpp"
#include "Frigga/Physics/PhysicsJointHandle.hpp"
#include "Frigga/Physics/PhysicsTypes.hpp"

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace FRIGGA_NAMESPACE
{

    class IPhysicsWorld;

    /// Gameplay-facing physics facade. Resolves RigidBody / CharacterController on entities.
    /// Backing engine (Jolt) stays behind IPhysicsWorld — plugins only see this type.
    ///
    /// Character ownership contract:
    /// - Physics owns world position, linear velocity, grounded/support, and capsule collision.
    /// - Gameplay owns Transform yaw/facing (and mesh/animation).
    /// - After each step, PhysicsSystem writes character position only and preserves ECS rotation.
    /// - Before each step, ECS facing is pushed into the CharacterVirtual.
    class Physics
    {
      public:
        Physics(const skr::Arc<fr::Registry> &registry, const skr::Arc<IPhysicsWorld> &world);
        ~Physics();

        Physics(const Physics &)            = delete;
        Physics &operator=(const Physics &) = delete;

        // --- Rigid bodies ---

        void SetKinematicPose(fr::Entity entity, const glm::vec3 &position,
                              const glm::quat &rotation);
        void SetLinearVelocity(fr::Entity entity, const glm::vec3 &velocity);
        [[nodiscard]] glm::vec3 GetLinearVelocity(fr::Entity entity) const;
        void SetAngularVelocity(fr::Entity entity, const glm::vec3 &velocity);
        [[nodiscard]] glm::vec3 GetAngularVelocity(fr::Entity entity) const;
        void AddImpulse(fr::Entity entity, const glm::vec3 &impulse);
        void AddForce(fr::Entity entity, const glm::vec3 &force);
        void AddTorque(fr::Entity entity, const glm::vec3 &torque);
        void AddAngularImpulse(fr::Entity entity, const glm::vec3 &impulse);

        // --- Character controllers ---

        void MoveCharacter(fr::Entity entity, const glm::vec3 &desiredWorldVelocity);
        void TeleportCharacter(fr::Entity entity, const glm::vec3 &worldPosition);
        void SetCharacterFacing(fr::Entity entity, const glm::quat &worldRotation);
        [[nodiscard]] bool IsCharacterGrounded(fr::Entity entity) const;
        [[nodiscard]] glm::vec3 GetCharacterVelocity(fr::Entity entity) const;
        [[nodiscard]] CharacterGroundInfo GetCharacterGroundInfo(fr::Entity entity) const;
        bool SetCharacterShape(fr::Entity entity, float radius, float height,
                               const glm::vec3 &centerOffset = {});
        void SetCharacterMaxStrength(fr::Entity entity, float maxStrength);

        // --- Joints ---

        PhysicsJointHandle CreateJoint(fr::Entity entityA, fr::Entity entityB,
                                       PhysicsJointType type,
                                       const glm::vec3 &localAnchorA = {},
                                       const glm::vec3 &localAnchorB = {},
                                       const glm::vec3 &hingeAxisLocalA = {0.0f, 1.0f, 0.0f});
        void DestroyJoint(PhysicsJointHandle handle);

        // --- Queries ---

        [[nodiscard]] RaycastHit Raycast(const glm::vec3 &origin, const glm::vec3 &direction,
                                         float maxDistance,
                                         const QueryFilter &filter = {}) const;
        [[nodiscard]] RaycastHit SphereCast(const glm::vec3 &origin, const glm::vec3 &direction,
                                            float radius, float maxDistance,
                                            const QueryFilter &filter = {}) const;
        [[nodiscard]] RaycastHit CapsuleCast(const glm::vec3 &origin, const glm::vec3 &direction,
                                             float radius, float height, float maxDistance,
                                             const QueryFilter &filter = {}) const;
        [[nodiscard]] std::vector<OverlapHit> OverlapSphere(const glm::vec3 &center, float radius,
                                                            const QueryFilter &filter = {}) const;
        [[nodiscard]] std::vector<OverlapHit> OverlapBox(const glm::vec3 &center,
                                                         const glm::vec3 &halfExtents,
                                                         const glm::quat &rotation,
                                                         const QueryFilter &filter = {}) const;

        // --- Events (previous fixed-step batch) ---

        [[nodiscard]] std::vector<TriggerEvent> DrainTriggerEvents();
        [[nodiscard]] std::vector<PhysicsContactEvent> DrainContactEvents();

        [[nodiscard]] float GetInterpolationAlpha() const;

      private:
        skr::Arc<fr::Registry> mRegistry;
        skr::Arc<IPhysicsWorld> mWorld;
    };

} // namespace FRIGGA_NAMESPACE
