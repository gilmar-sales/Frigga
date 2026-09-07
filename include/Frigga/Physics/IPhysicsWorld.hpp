#pragma once

#include "Frigga/Physics/PhysicsBodyHandle.hpp"
#include "Frigga/Physics/PhysicsCharacterHandle.hpp"
#include "Frigga/Physics/PhysicsJointHandle.hpp"
#include "Frigga/Physics/PhysicsTypes.hpp"

#include <cstdint>
#include <functional>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace FRIGGA_NAMESPACE
{

    class IPhysicsWorld
    {
      public:
        virtual ~IPhysicsWorld() = default;

        virtual void Clear() = 0;
        virtual void OptimizeBroadPhase() = 0;
        virtual void Step(float deltaTime) = 0;
        /// Advance exactly one (or more) fixed simulation ticks, ignoring the accumulator.
        virtual void StepFixed(int steps = 1) = 0;
        [[nodiscard]] virtual float GetFixedDeltaTime() const = 0;
        /// Residual accumulator / fixedDt after the last Step (0 after StepFixed).
        [[nodiscard]] virtual float GetInterpolationAlpha() const = 0;

        virtual PhysicsBodyHandle CreateBody(const PhysicsBodyDesc &desc) = 0;
        virtual void DestroyBody(PhysicsBodyHandle handle) = 0;

        virtual void SetTransform(PhysicsBodyHandle handle, const glm::vec3 &position,
                                  const glm::quat &rotation) = 0;
        virtual void GetTransform(PhysicsBodyHandle handle, glm::vec3 &position,
                                  glm::quat &rotation) const = 0;
        [[nodiscard]] virtual bool GetInterpolatedBodyPose(PhysicsBodyHandle handle, float alpha,
                                                           glm::vec3 &position,
                                                           glm::quat &rotation) const = 0;

        virtual void SetLinearVelocity(PhysicsBodyHandle handle, const glm::vec3 &velocity) = 0;
        [[nodiscard]] virtual glm::vec3 GetLinearVelocity(PhysicsBodyHandle handle) const = 0;
        virtual void SetAngularVelocity(PhysicsBodyHandle handle, const glm::vec3 &velocity) = 0;
        [[nodiscard]] virtual glm::vec3 GetAngularVelocity(PhysicsBodyHandle handle) const = 0;
        virtual void AddImpulse(PhysicsBodyHandle handle, const glm::vec3 &impulse) = 0;
        virtual void AddForce(PhysicsBodyHandle handle, const glm::vec3 &force) = 0;
        virtual void AddTorque(PhysicsBodyHandle handle, const glm::vec3 &torque) = 0;
        virtual void AddAngularImpulse(PhysicsBodyHandle handle, const glm::vec3 &impulse) = 0;

        virtual PhysicsCharacterHandle CreateCharacter(const PhysicsCharacterDesc &desc) = 0;
        virtual void DestroyCharacter(PhysicsCharacterHandle handle) = 0;
        virtual void BindCharacter(std::uint64_t entity, PhysicsCharacterHandle handle) = 0;
        virtual void UnbindCharacter(std::uint64_t entity) = 0;
        [[nodiscard]] virtual PhysicsCharacterHandle FindCharacter(std::uint64_t entity) const = 0;
        virtual void ForEachCharacter(
            const std::function<void(std::uint64_t, PhysicsCharacterHandle)> &visit) const = 0;
        virtual void SetCharacterVelocity(PhysicsCharacterHandle handle,
                                          const glm::vec3 &velocity) = 0;
        [[nodiscard]] virtual glm::vec3 GetCharacterVelocity(
            PhysicsCharacterHandle handle) const = 0;
        virtual void GetCharacterTransform(PhysicsCharacterHandle handle, glm::vec3 &position,
                                           glm::quat &rotation) const = 0;
        [[nodiscard]] virtual bool GetInterpolatedCharacterPosition(
            PhysicsCharacterHandle handle, float alpha, glm::vec3 &position) const = 0;
        virtual void SetCharacterPosition(PhysicsCharacterHandle handle,
                                          const glm::vec3 &position) = 0;
        virtual void SetCharacterRotation(PhysicsCharacterHandle handle,
                                          const glm::quat &rotation) = 0;
        [[nodiscard]] virtual bool IsCharacterGrounded(PhysicsCharacterHandle handle) const = 0;
        [[nodiscard]] virtual CharacterGroundInfo GetCharacterGroundInfo(
            PhysicsCharacterHandle handle) const = 0;
        virtual bool SetCharacterShape(PhysicsCharacterHandle handle,
                                       const PhysicsCharacterShapeDesc &shape) = 0;
        virtual void SetCharacterMaxStrength(PhysicsCharacterHandle handle, float maxStrength) = 0;

        virtual PhysicsJointHandle CreateJoint(const PhysicsJointDesc &desc) = 0;
        virtual void DestroyJoint(PhysicsJointHandle handle) = 0;

        [[nodiscard]] virtual RaycastHit Raycast(const glm::vec3 &origin,
                                                 const glm::vec3 &direction, float maxDistance,
                                                 const QueryFilter &filter = {}) const = 0;
        [[nodiscard]] virtual RaycastHit SphereCast(const glm::vec3 &origin,
                                                    const glm::vec3 &direction, float radius,
                                                    float maxDistance,
                                                    const QueryFilter &filter = {}) const = 0;
        [[nodiscard]] virtual RaycastHit CapsuleCast(const glm::vec3 &origin,
                                                     const glm::vec3 &direction, float radius,
                                                     float height, float maxDistance,
                                                     const QueryFilter &filter = {}) const = 0;
        [[nodiscard]] virtual std::vector<OverlapHit> OverlapSphere(
            const glm::vec3 &center, float radius, const QueryFilter &filter = {}) const = 0;
        [[nodiscard]] virtual std::vector<OverlapHit> OverlapBox(
            const glm::vec3 &center, const glm::vec3 &halfExtents, const glm::quat &rotation,
            const QueryFilter &filter = {}) const = 0;

        [[nodiscard]] virtual std::vector<TriggerEvent> DrainTriggerEvents() = 0;
        [[nodiscard]] virtual std::vector<PhysicsContactEvent> DrainContactEvents() = 0;

        virtual void SetGravity(const glm::vec3 &gravity) = 0;
        [[nodiscard]] virtual glm::vec3 GetGravity() const = 0;

        [[nodiscard]] virtual bool IsBodyActive(PhysicsBodyHandle handle) const = 0;
    };

} // namespace FRIGGA_NAMESPACE
