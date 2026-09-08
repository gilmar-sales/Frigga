#pragma once

#include "Frigga/Macro.hpp"
#include "Frigga/Physics/IPhysicsWorld.hpp"

#include <array>
#include <memory>
#include <unordered_map>

namespace FRIGGA_NAMESPACE
{

    class JoltPhysicsWorld final: public IPhysicsWorld
    {
      public:
        JoltPhysicsWorld();
        ~JoltPhysicsWorld() override;

        JoltPhysicsWorld(const JoltPhysicsWorld &)            = delete;
        JoltPhysicsWorld &operator=(const JoltPhysicsWorld &) = delete;

        void Clear() override;
        void OptimizeBroadPhase() override;
        void Step(float deltaTime) override;
        void StepFixed(int steps = 1) override;
        [[nodiscard]] float GetFixedDeltaTime() const override;
        [[nodiscard]] float GetInterpolationAlpha() const override;

        PhysicsBodyHandle CreateBody(const PhysicsBodyDesc &desc) override;
        void DestroyBody(PhysicsBodyHandle handle) override;

        void SetTransform(PhysicsBodyHandle handle, const glm::vec3 &position,
                          const glm::quat &rotation) override;
        void GetTransform(PhysicsBodyHandle handle, glm::vec3 &position,
                          glm::quat &rotation) const override;
        [[nodiscard]] bool GetInterpolatedBodyPose(PhysicsBodyHandle handle, float alpha,
                                                    glm::vec3 &position,
                                                    glm::quat &rotation) const override;

        void SetLinearVelocity(PhysicsBodyHandle handle, const glm::vec3 &velocity) override;
        [[nodiscard]] glm::vec3 GetLinearVelocity(PhysicsBodyHandle handle) const override;
        void SetAngularVelocity(PhysicsBodyHandle handle, const glm::vec3 &velocity) override;
        [[nodiscard]] glm::vec3 GetAngularVelocity(PhysicsBodyHandle handle) const override;
        void AddImpulse(PhysicsBodyHandle handle, const glm::vec3 &impulse) override;
        void AddForce(PhysicsBodyHandle handle, const glm::vec3 &force) override;
        void AddTorque(PhysicsBodyHandle handle, const glm::vec3 &torque) override;
        void AddAngularImpulse(PhysicsBodyHandle handle, const glm::vec3 &impulse) override;

        PhysicsJointHandle CreateJoint(const PhysicsJointDesc &desc) override;
        void DestroyJoint(PhysicsJointHandle handle) override;

        [[nodiscard]] RaycastHit Raycast(const glm::vec3 &origin, const glm::vec3 &direction,
                                         float maxDistance,
                                         const QueryFilter &filter = {}) const override;
        [[nodiscard]] RaycastHit SphereCast(const glm::vec3 &origin, const glm::vec3 &direction,
                                            float radius, float maxDistance,
                                            const QueryFilter &filter = {}) const override;
        [[nodiscard]] RaycastHit CapsuleCast(const glm::vec3 &origin, const glm::vec3 &direction,
                                             float radius, float height, float maxDistance,
                                             const QueryFilter &filter = {}) const override;
        [[nodiscard]] std::vector<OverlapHit> OverlapSphere(
            const glm::vec3 &center, float radius, const QueryFilter &filter = {}) const override;
        [[nodiscard]] std::vector<OverlapHit> OverlapBox(const glm::vec3 &center,
                                                         const glm::vec3 &halfExtents,
                                                         const glm::quat &rotation,
                                                         const QueryFilter &filter = {}) const override;

        [[nodiscard]] std::vector<TriggerEvent> DrainTriggerEvents() override;
        [[nodiscard]] std::vector<PhysicsContactEvent> DrainContactEvents() override;

        void SetGravity(const glm::vec3 &gravity) override;
        [[nodiscard]] glm::vec3 GetGravity() const override;

        [[nodiscard]] bool IsBodyActive(PhysicsBodyHandle handle) const override;

      private:
        void stepFixedInternal(int steps);
        void updateBodyInterpolationSamples();
        void flushPendingEvents();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };

} // namespace FRIGGA_NAMESPACE
