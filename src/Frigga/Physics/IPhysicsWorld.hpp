#pragma once

#include "Frigga/Physics/PhysicsBodyHandle.hpp"
#include "Frigga/Physics/PhysicsTypes.hpp"

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

        virtual PhysicsBodyHandle CreateBody(const PhysicsBodyDesc &desc) = 0;
        virtual void DestroyBody(PhysicsBodyHandle handle) = 0;

        virtual void SetTransform(PhysicsBodyHandle handle, const glm::vec3 &position,
                                  const glm::quat &rotation) = 0;
        virtual void GetTransform(PhysicsBodyHandle handle, glm::vec3 &position,
                                  glm::quat &rotation) const = 0;

        virtual void SetGravity(const glm::vec3 &gravity) = 0;
        [[nodiscard]] virtual glm::vec3 GetGravity() const = 0;
    };

} // namespace FRIGGA_NAMESPACE
