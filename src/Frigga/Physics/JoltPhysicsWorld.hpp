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

        PhysicsBodyHandle CreateBody(const PhysicsBodyDesc &desc) override;
        void DestroyBody(PhysicsBodyHandle handle) override;

        void SetTransform(PhysicsBodyHandle handle, const glm::vec3 &position,
                          const glm::quat &rotation) override;
        void GetTransform(PhysicsBodyHandle handle, glm::vec3 &position,
                          glm::quat &rotation) const override;

        void SetLinearVelocity(PhysicsBodyHandle handle, const glm::vec3 &velocity) override;
        [[nodiscard]] glm::vec3 GetLinearVelocity(PhysicsBodyHandle handle) const override;
        void AddImpulse(PhysicsBodyHandle handle, const glm::vec3 &impulse) override;
        void AddForce(PhysicsBodyHandle handle, const glm::vec3 &force) override;

        PhysicsCharacterHandle CreateCharacter(const PhysicsCharacterDesc &desc) override;
        void DestroyCharacter(PhysicsCharacterHandle handle) override;
        void SetCharacterVelocity(PhysicsCharacterHandle handle,
                                  const glm::vec3 &velocity) override;
        [[nodiscard]] glm::vec3 GetCharacterVelocity(PhysicsCharacterHandle handle) const override;
        void GetCharacterTransform(PhysicsCharacterHandle handle, glm::vec3 &position,
                                   glm::quat &rotation) const override;
        [[nodiscard]] bool IsCharacterGrounded(PhysicsCharacterHandle handle) const override;

        void SetGravity(const glm::vec3 &gravity) override;
        [[nodiscard]] glm::vec3 GetGravity() const override;

        [[nodiscard]] bool IsBodyActive(PhysicsBodyHandle handle) const override;

      private:
        void stepFixedInternal(int steps);
        void updateCharactersFixed();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };

} // namespace FRIGGA_NAMESPACE
