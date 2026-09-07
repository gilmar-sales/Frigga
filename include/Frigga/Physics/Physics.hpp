#pragma once

#include "Frigga/Macro.hpp"
#include "Frigga/Physics/PhysicsTypes.hpp"

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

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
        void AddImpulse(fr::Entity entity, const glm::vec3 &impulse);
        void AddForce(fr::Entity entity, const glm::vec3 &force);

        // --- Character controllers ---

        /// Desired world-space linear velocity for the next physics step(s).
        void MoveCharacter(fr::Entity entity, const glm::vec3 &desiredWorldVelocity);
        /// Instantly place the character and sync Transform position (preserves facing).
        void TeleportCharacter(fr::Entity entity, const glm::vec3 &worldPosition);
        /// Set gameplay facing on Transform and push it into the CharacterVirtual.
        void SetCharacterFacing(fr::Entity entity, const glm::quat &worldRotation);
        [[nodiscard]] bool IsCharacterGrounded(fr::Entity entity) const;
        [[nodiscard]] glm::vec3 GetCharacterVelocity(fr::Entity entity) const;
        [[nodiscard]] CharacterGroundInfo GetCharacterGroundInfo(fr::Entity entity) const;
        /// Resize the capsule (crouch / stance). Returns false if blocked by penetration.
        bool SetCharacterShape(fr::Entity entity, float radius, float height,
                               const glm::vec3 &centerOffset = {});
        /// Max push force against dynamic bodies (Newtons).
        void SetCharacterMaxStrength(fr::Entity entity, float maxStrength);

      private:
        skr::Arc<fr::Registry> mRegistry;
        skr::Arc<IPhysicsWorld> mWorld;
    };

} // namespace FRIGGA_NAMESPACE
