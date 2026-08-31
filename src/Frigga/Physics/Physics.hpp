#pragma once

#include "Frigga/Macro.hpp"

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace FRIGGA_NAMESPACE
{

    class IPhysicsWorld;

    /// Gameplay-facing physics facade. Resolves RigidBody / CharacterController on entities.
    /// Backing engine (Jolt) stays behind IPhysicsWorld — plugins only see this type.
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
        [[nodiscard]] bool IsCharacterGrounded(fr::Entity entity) const;
        [[nodiscard]] glm::vec3 GetCharacterVelocity(fr::Entity entity) const;

      private:
        skr::Arc<fr::Registry> mRegistry;
        skr::Arc<IPhysicsWorld> mWorld;
    };

} // namespace FRIGGA_NAMESPACE
