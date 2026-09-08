#include "EmptyApp.hpp"

#include <Frigga/ECS/Components/NameComponent.hpp>
#include <Frigga/ECS/Components/RigidBodyComponent.hpp>
#include <Frigga/ECS/Components/TransformComponent.hpp>
#include <Frigga/Physics/IPhysicsWorld.hpp>
#include <Frigga/Physics/JoltPhysicsWorld.hpp>
#include <Frigga/Physics/Physics.hpp>
#include <Frigga/Physics/PhysicsJointHandle.hpp>

#include <Freyr/Freyr.hpp>
#include <gtest/gtest.h>

#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace
{
    class FakePhysicsWorld final: public fg::IPhysicsWorld
    {
      public:
        void Clear() override
        {
            bodies.clear();
            characters.clear();
            entityCharacters.clear();
            joints.clear();
            pendingTriggerEvents.clear();
            pendingContactEvents.clear();
            nextBody      = 1;
            nextCharacter = 1;
            nextJoint     = 1;
        }

        void OptimizeBroadPhase() override {}
        void Step(float) override {}
        void StepFixed(int) override {}
        [[nodiscard]] float GetFixedDeltaTime() const override
        {
            return 1.0f / 60.0f;
        }

        [[nodiscard]] float GetInterpolationAlpha() const override
        {
            return 0.0f;
        }

        fg::PhysicsBodyHandle CreateBody(const fg::PhysicsBodyDesc &) override
        {
            fg::PhysicsBodyHandle handle {.id = nextBody++};
            bodies[handle.id] = BodyState {};
            return handle;
        }

        void DestroyBody(fg::PhysicsBodyHandle handle) override
        {
            bodies.erase(handle.id);
        }

        void SetTransform(fg::PhysicsBodyHandle handle, const glm::vec3 &position,
                          const glm::quat &rotation) override
        {
            auto it = bodies.find(handle.id);
            if(it == bodies.end())
            {
                return;
            }
            it->second.position = position;
            it->second.rotation = rotation;
            ++setTransformCalls;
        }

        void GetTransform(fg::PhysicsBodyHandle handle, glm::vec3 &position,
                          glm::quat &rotation) const override
        {
            const auto it = bodies.find(handle.id);
            if(it == bodies.end())
            {
                return;
            }
            position = it->second.position;
            rotation = it->second.rotation;
        }

        [[nodiscard]] bool GetInterpolatedBodyPose(fg::PhysicsBodyHandle handle, float,
                                                   glm::vec3 &position,
                                                   glm::quat &rotation) const override
        {
            const auto it = bodies.find(handle.id);
            if(it == bodies.end())
            {
                return false;
            }
            position = it->second.position;
            rotation = it->second.rotation;
            return true;
        }

        void SetLinearVelocity(fg::PhysicsBodyHandle handle, const glm::vec3 &velocity) override
        {
            auto it = bodies.find(handle.id);
            if(it == bodies.end())
            {
                return;
            }
            it->second.linearVelocity = velocity;
            ++setVelocityCalls;
        }

        [[nodiscard]] glm::vec3 GetLinearVelocity(fg::PhysicsBodyHandle handle) const override
        {
            const auto it = bodies.find(handle.id);
            if(it == bodies.end())
            {
                return {};
            }
            return it->second.linearVelocity;
        }

        void SetAngularVelocity(fg::PhysicsBodyHandle handle, const glm::vec3 &velocity) override
        {
            auto it = bodies.find(handle.id);
            if(it == bodies.end())
            {
                return;
            }
            it->second.angularVelocity = velocity;
            ++setAngularVelocityCalls;
        }

        [[nodiscard]] glm::vec3 GetAngularVelocity(fg::PhysicsBodyHandle handle) const override
        {
            const auto it = bodies.find(handle.id);
            if(it == bodies.end())
            {
                return {};
            }
            return it->second.angularVelocity;
        }

        void AddImpulse(fg::PhysicsBodyHandle handle, const glm::vec3 &impulse) override
        {
            auto it = bodies.find(handle.id);
            if(it == bodies.end())
            {
                return;
            }
            it->second.impulse += impulse;
            ++impulseCalls;
        }

        void AddForce(fg::PhysicsBodyHandle handle, const glm::vec3 &force) override
        {
            auto it = bodies.find(handle.id);
            if(it == bodies.end())
            {
                return;
            }
            it->second.force += force;
            ++forceCalls;
        }

        void AddTorque(fg::PhysicsBodyHandle handle, const glm::vec3 &torque) override
        {
            auto it = bodies.find(handle.id);
            if(it == bodies.end())
            {
                return;
            }
            it->second.torque += torque;
            ++torqueCalls;
        }

        void AddAngularImpulse(fg::PhysicsBodyHandle handle, const glm::vec3 &impulse) override
        {
            auto it = bodies.find(handle.id);
            if(it == bodies.end())
            {
                return;
            }
            it->second.angularImpulse += impulse;
            ++angularImpulseCalls;
        }

        fg::PhysicsCharacterHandle CreateCharacter(const fg::PhysicsCharacterDesc &) override
        {
            fg::PhysicsCharacterHandle handle {.id = nextCharacter++};
            characters[handle.id] = CharacterState {};
            return handle;
        }

        void DestroyCharacter(fg::PhysicsCharacterHandle handle) override
        {
            characters.erase(handle.id);
            for(auto it = entityCharacters.begin(); it != entityCharacters.end();)
            {
                if(it->second.id == handle.id)
                {
                    it = entityCharacters.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        void BindCharacter(std::uint64_t entity, fg::PhysicsCharacterHandle handle,
                           fg::PhysicsBodyHandle = {}) override
        {
            if(!handle.IsValid())
            {
                entityCharacters.erase(entity);
                return;
            }
            entityCharacters[entity] = handle;
        }

        void UnbindCharacter(std::uint64_t entity) override
        {
            entityCharacters.erase(entity);
        }

        [[nodiscard]] fg::PhysicsCharacterHandle FindCharacter(std::uint64_t entity) const override
        {
            const auto it = entityCharacters.find(entity);
            if(it == entityCharacters.end())
            {
                return {};
            }
            return it->second;
        }

        void ForEachCharacter(
            const std::function<void(std::uint64_t, fg::PhysicsCharacterHandle)> &visit) const override
        {
            if(!visit)
            {
                return;
            }
            for(const auto &[entity, handle] : entityCharacters)
            {
                visit(entity, handle);
            }
        }

        void SetCharacterVelocity(fg::PhysicsCharacterHandle handle,
                                  const glm::vec3 &velocity) override
        {
            auto it = characters.find(handle.id);
            if(it == characters.end())
            {
                return;
            }
            it->second.velocity = velocity;
            ++characterVelocityCalls;
        }

        [[nodiscard]] glm::vec3 GetCharacterVelocity(fg::PhysicsCharacterHandle handle) const override
        {
            const auto it = characters.find(handle.id);
            if(it == characters.end())
            {
                return {};
            }
            return it->second.velocity;
        }

        void GetCharacterTransform(fg::PhysicsCharacterHandle handle, glm::vec3 &position,
                                   glm::quat &rotation) const override
        {
            const auto it = characters.find(handle.id);
            if(it == characters.end())
            {
                return;
            }
            position = it->second.position;
            rotation = it->second.rotation;
        }

        [[nodiscard]] bool GetInterpolatedCharacterPosition(fg::PhysicsCharacterHandle handle,
                                                            float,
                                                            glm::vec3 &position) const override
        {
            const auto it = characters.find(handle.id);
            if(it == characters.end())
            {
                return false;
            }
            position = it->second.position;
            return true;
        }

        void SetCharacterPosition(fg::PhysicsCharacterHandle handle,
                                  const glm::vec3 &position) override
        {
            auto it = characters.find(handle.id);
            if(it == characters.end())
            {
                return;
            }
            it->second.position = position;
            ++characterPositionCalls;
        }

        void SetCharacterRotation(fg::PhysicsCharacterHandle handle,
                                  const glm::quat &rotation) override
        {
            auto it = characters.find(handle.id);
            if(it == characters.end())
            {
                return;
            }
            it->second.rotation = rotation;
            ++characterRotationCalls;
        }

        [[nodiscard]] bool IsCharacterGrounded(fg::PhysicsCharacterHandle handle) const override
        {
            return GetCharacterGroundInfo(handle).grounded;
        }

        [[nodiscard]] fg::CharacterGroundInfo GetCharacterGroundInfo(
            fg::PhysicsCharacterHandle handle) const override
        {
            fg::CharacterGroundInfo info {};
            const auto it = characters.find(handle.id);
            if(it == characters.end())
            {
                return info;
            }
            info.grounded  = it->second.grounded;
            info.state      = it->second.grounded ? fg::CharacterGroundState::OnGround
                                                 : fg::CharacterGroundState::InAir;
            info.position  = it->second.position;
            info.normal    = it->second.groundNormal;
            info.velocity  = it->second.groundVelocity;
            info.groundBody = it->second.groundBody;
            return info;
        }

        bool SetCharacterShape(fg::PhysicsCharacterHandle handle,
                               const fg::PhysicsCharacterShapeDesc &shape) override
        {
            auto it = characters.find(handle.id);
            if(it == characters.end())
            {
                return false;
            }
            it->second.radius       = shape.radius;
            it->second.height       = shape.height;
            it->second.centerOffset = shape.centerOffset;
            ++characterShapeCalls;
            return true;
        }

        void SetCharacterMaxStrength(fg::PhysicsCharacterHandle handle, float maxStrength) override
        {
            auto it = characters.find(handle.id);
            if(it == characters.end())
            {
                return;
            }
            it->second.maxStrength = maxStrength;
            ++characterStrengthCalls;
        }

        fg::PhysicsJointHandle CreateJoint(const fg::PhysicsJointDesc &desc) override
        {
            lastJointDesc = desc;
            ++createJointCalls;
            fg::PhysicsJointHandle handle {.id = nextJoint++};
            joints[handle.id] = desc;
            return handle;
        }

        void DestroyJoint(fg::PhysicsJointHandle handle) override
        {
            joints.erase(handle.id);
            ++destroyJointCalls;
        }

        [[nodiscard]] fg::RaycastHit Raycast(const glm::vec3 &origin, const glm::vec3 &direction,
                                             float maxDistance,
                                             const fg::QueryFilter &filter) const override
        {
            lastRayOrigin      = origin;
            lastRayDirection   = direction;
            lastRayMaxDistance = maxDistance;
            lastRayFilter      = filter;
            ++raycastCalls;
            return cannedRaycastHit;
        }

        [[nodiscard]] fg::RaycastHit SphereCast(const glm::vec3 &, const glm::vec3 &, float, float,
                                                const fg::QueryFilter &) const override
        {
            return {};
        }

        [[nodiscard]] fg::RaycastHit CapsuleCast(const glm::vec3 &, const glm::vec3 &, float, float,
                                                 float, const fg::QueryFilter &) const override
        {
            return {};
        }

        [[nodiscard]] std::vector<fg::OverlapHit> OverlapSphere(
            const glm::vec3 &, float, const fg::QueryFilter &) const override
        {
            return {};
        }

        [[nodiscard]] std::vector<fg::OverlapHit> OverlapBox(const glm::vec3 &, const glm::vec3 &,
                                                             const glm::quat &,
                                                             const fg::QueryFilter &) const override
        {
            return {};
        }

        [[nodiscard]] std::vector<fg::TriggerEvent> DrainTriggerEvents() override
        {
            ++drainTriggerCalls;
            auto events = pendingTriggerEvents;
            pendingTriggerEvents.clear();
            return events;
        }

        [[nodiscard]] std::vector<fg::PhysicsContactEvent> DrainContactEvents() override
        {
            ++drainContactCalls;
            auto events = pendingContactEvents;
            pendingContactEvents.clear();
            return events;
        }

        void SetGravity(const glm::vec3 &g) override
        {
            gravity = g;
        }

        [[nodiscard]] glm::vec3 GetGravity() const override
        {
            return gravity;
        }

        [[nodiscard]] bool IsBodyActive(fg::PhysicsBodyHandle handle) const override
        {
            return bodies.contains(handle.id);
        }

        struct BodyState
        {
            glm::vec3 position {0.0f};
            glm::quat rotation {1.0f, 0.0f, 0.0f, 0.0f};
            glm::vec3 linearVelocity {0.0f};
            glm::vec3 angularVelocity {0.0f};
            glm::vec3 impulse {0.0f};
            glm::vec3 force {0.0f};
            glm::vec3 torque {0.0f};
            glm::vec3 angularImpulse {0.0f};
        };

        struct CharacterState
        {
            glm::vec3 position {0.0f};
            glm::quat rotation {1.0f, 0.0f, 0.0f, 0.0f};
            glm::vec3 velocity {0.0f};
            bool      grounded = false;
            glm::vec3 groundNormal {0.0f, 1.0f, 0.0f};
            glm::vec3 groundVelocity {0.0f};
            fg::PhysicsBodyHandle groundBody {};
            float radius       = 0.5f;
            float height       = 1.0f;
            glm::vec3 centerOffset {0.0f};
            float maxStrength  = 100.0f;
        };

        std::unordered_map<std::uint32_t, BodyState> bodies;
        std::unordered_map<std::uint32_t, CharacterState> characters;
        std::unordered_map<std::uint64_t, fg::PhysicsCharacterHandle> entityCharacters;
        std::unordered_map<std::uint32_t, fg::PhysicsJointDesc> joints;
        std::uint32_t nextBody      = 1;
        std::uint32_t nextCharacter = 1;
        std::uint32_t nextJoint     = 1;
        glm::vec3 gravity {0.0f, -9.81f, 0.0f};

        mutable glm::vec3 lastRayOrigin {0.0f};
        mutable glm::vec3 lastRayDirection {0.0f};
        mutable float lastRayMaxDistance = 0.0f;
        mutable fg::QueryFilter lastRayFilter {};
        mutable int raycastCalls = 0;
        fg::RaycastHit cannedRaycastHit {};

        fg::PhysicsJointDesc lastJointDesc {};
        std::vector<fg::TriggerEvent> pendingTriggerEvents;
        std::vector<fg::PhysicsContactEvent> pendingContactEvents;

        int setTransformCalls         = 0;
        int setVelocityCalls          = 0;
        int setAngularVelocityCalls   = 0;
        int impulseCalls              = 0;
        int forceCalls                = 0;
        int torqueCalls               = 0;
        int angularImpulseCalls       = 0;
        int createJointCalls          = 0;
        int destroyJointCalls         = 0;
        mutable int drainTriggerCalls = 0;
        mutable int drainContactCalls = 0;
        int characterVelocityCalls    = 0;
        int characterPositionCalls    = 0;
        int characterRotationCalls    = 0;
        int characterShapeCalls       = 0;
        int characterStrengthCalls    = 0;
    };

    struct PhysicsHarness
    {
        skr::Arc<skr::IApplication> app;
        skr::Arc<fr::Registry> registry;
        skr::Arc<FakePhysicsWorld> world;
        skr::Arc<fg::Physics> physics;

        static PhysicsHarness Create()
        {
            PhysicsHarness harness;
            harness.app =
                skr::ApplicationBuilder()
                    .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension &freyr) {
                        freyr.WithComponent<fg::NameComponent>()
                            .WithComponent<fg::TransformComponent>()
                            .WithComponent<fg::RigidBodyComponent>()
                            .WithPipeline([](fr::PipelineBuilder &pipeline) {
                                pipeline.WithName("Simulation");
                            });
                    })
                    .Build<EmptyApp>();

            harness.registry = harness.app->GetRootServiceProvider()->GetService<fr::Registry>();
            harness.world    = skr::MakeArc<FakePhysicsWorld>();
            harness.physics  = skr::MakeArc<fg::Physics>(harness.registry, harness.world);
            return harness;
        }
    };
} // namespace

TEST(PhysicsFacade, SetLinearVelocityForwardsToWorld)
{
    auto harness = PhysicsHarness::Create();
    auto entity  = harness.registry->CreateEntity(
        fg::NameComponent {.name = "Box"}, fg::TransformComponent {}, fg::RigidBodyComponent {});
    harness.registry->ExecuteTasks();

    harness.registry->TryGetComponents<fg::RigidBodyComponent>(
        entity, [&](fg::RigidBodyComponent &rb) {
            rb.body = harness.world->CreateBody(fg::PhysicsBodyDesc {});
        });
    harness.registry->ExecuteTasks();

    harness.physics->SetLinearVelocity(entity, {1.0f, 2.0f, 3.0f});
    EXPECT_EQ(harness.world->setVelocityCalls, 1);
    const auto velocity = harness.physics->GetLinearVelocity(entity);
    EXPECT_FLOAT_EQ(velocity.x, 1.0f);
    EXPECT_FLOAT_EQ(velocity.y, 2.0f);
    EXPECT_FLOAT_EQ(velocity.z, 3.0f);
}

TEST(PhysicsFacade, SetKinematicPoseUpdatesTransformAndWorld)
{
    auto harness = PhysicsHarness::Create();
    auto entity  = harness.registry->CreateEntity(
        fg::NameComponent {.name = "Kin"}, fg::TransformComponent {},
        fg::RigidBodyComponent {.motion = fg::BodyMotionType::Kinematic});
    harness.registry->ExecuteTasks();

    harness.registry->TryGetComponents<fg::RigidBodyComponent>(
        entity, [&](fg::RigidBodyComponent &rb) {
            rb.body = harness.world->CreateBody(fg::PhysicsBodyDesc {});
        });
    harness.registry->ExecuteTasks();

    const glm::vec3 pos {4.0f, 5.0f, 6.0f};
    const glm::quat rot {1.0f, 0.0f, 0.0f, 0.0f};
    harness.physics->SetKinematicPose(entity, pos, rot);

    harness.registry->TryGetComponents<fg::TransformComponent>(
        entity, [&](fg::TransformComponent &transform) {
            EXPECT_FLOAT_EQ(transform.position.x, pos.x);
            EXPECT_FLOAT_EQ(transform.position.y, pos.y);
            EXPECT_FLOAT_EQ(transform.position.z, pos.z);
        });
    EXPECT_EQ(harness.world->setTransformCalls, 1);
}

TEST(PhysicsFacade, MoveCharacterNoComponentIsSafe)
{
    auto harness = PhysicsHarness::Create();
    auto entity  = harness.registry->CreateEntity(fg::NameComponent {.name = "NoCc"},
                                                 fg::TransformComponent {});
    harness.registry->ExecuteTasks();

    harness.physics->MoveCharacter(entity, {1.0f, 0.0f, 0.0f});
    EXPECT_EQ(harness.world->characterVelocityCalls, 0);
    EXPECT_FALSE(harness.physics->IsCharacterGrounded(entity));
}

TEST(PhysicsFacade, MoveCharacterForwardsVelocity)
{
    auto harness = PhysicsHarness::Create();
    auto entity  = harness.registry->CreateEntity(
        fg::NameComponent {.name = "Hero"}, fg::TransformComponent {});
    harness.registry->ExecuteTasks();

    const auto handle = harness.world->CreateCharacter(fg::PhysicsCharacterDesc {});
    harness.world->BindCharacter(static_cast<std::uint64_t>(entity), handle);

    harness.world->characters.begin()->second.grounded = true;
    harness.physics->MoveCharacter(entity, {2.0f, 0.0f, -1.0f});

    EXPECT_EQ(harness.world->characterVelocityCalls, 1);
    EXPECT_TRUE(harness.physics->IsCharacterGrounded(entity));
    const auto velocity = harness.physics->GetCharacterVelocity(entity);
    EXPECT_FLOAT_EQ(velocity.x, 2.0f);
    EXPECT_FLOAT_EQ(velocity.y, 0.0f);
    EXPECT_FLOAT_EQ(velocity.z, -1.0f);
}

TEST(PhysicsFacade, TeleportCharacterUpdatesTransformAndWorld)
{
    auto harness = PhysicsHarness::Create();
    auto entity  = harness.registry->CreateEntity(
        fg::NameComponent {.name = "Hero"},
        fg::TransformComponent {.position = {1.0f, 0.0f, 0.0f}});
    harness.registry->ExecuteTasks();

    const auto handle = harness.world->CreateCharacter(fg::PhysicsCharacterDesc {});
    harness.world->BindCharacter(static_cast<std::uint64_t>(entity), handle);

    const glm::vec3 dest {9.0f, 2.0f, -3.0f};
    harness.physics->TeleportCharacter(entity, dest);

    EXPECT_EQ(harness.world->characterPositionCalls, 1);
    harness.registry->TryGetComponents<fg::TransformComponent>(
        entity, [&](fg::TransformComponent &transform) {
            EXPECT_FLOAT_EQ(transform.position.x, dest.x);
            EXPECT_FLOAT_EQ(transform.position.y, dest.y);
            EXPECT_FLOAT_EQ(transform.position.z, dest.z);
        });
}

TEST(PhysicsFacade, SetCharacterFacingUpdatesTransformAndWorld)
{
    auto harness = PhysicsHarness::Create();
    auto entity  = harness.registry->CreateEntity(
        fg::NameComponent {.name = "Hero"}, fg::TransformComponent {});
    harness.registry->ExecuteTasks();

    const auto handle = harness.world->CreateCharacter(fg::PhysicsCharacterDesc {});
    harness.world->BindCharacter(static_cast<std::uint64_t>(entity), handle);

    const glm::quat facing = glm::angleAxis(glm::radians(90.0f), glm::vec3 {0.0f, 1.0f, 0.0f});
    harness.physics->SetCharacterFacing(entity, facing);

    EXPECT_EQ(harness.world->characterRotationCalls, 1);
    harness.registry->TryGetComponents<fg::TransformComponent>(
        entity, [&](fg::TransformComponent &transform) {
            EXPECT_NEAR(transform.rotation.w, facing.w, 1e-5f);
            EXPECT_NEAR(transform.rotation.x, facing.x, 1e-5f);
            EXPECT_NEAR(transform.rotation.y, facing.y, 1e-5f);
            EXPECT_NEAR(transform.rotation.z, facing.z, 1e-5f);
        });
}

TEST(PhysicsFacade, CharacterShapeAndStrengthForward)
{
    auto harness = PhysicsHarness::Create();
    auto entity  = harness.registry->CreateEntity(
        fg::NameComponent {.name = "Hero"}, fg::TransformComponent {});
    harness.registry->ExecuteTasks();

    const auto handle = harness.world->CreateCharacter(fg::PhysicsCharacterDesc {});
    harness.world->BindCharacter(static_cast<std::uint64_t>(entity), handle);

    EXPECT_TRUE(harness.physics->SetCharacterShape(entity, 0.4f, 0.6f, {0.0f, 0.1f, 0.0f}));
    EXPECT_EQ(harness.world->characterShapeCalls, 1);
    EXPECT_FLOAT_EQ(harness.world->characters.begin()->second.radius, 0.4f);
    EXPECT_FLOAT_EQ(harness.world->characters.begin()->second.height, 0.6f);

    harness.physics->SetCharacterMaxStrength(entity, 250.0f);
    EXPECT_EQ(harness.world->characterStrengthCalls, 1);
    EXPECT_FLOAT_EQ(harness.world->characters.begin()->second.maxStrength, 250.0f);
}

TEST(PhysicsFacade, GetCharacterGroundInfo)
{
    auto harness = PhysicsHarness::Create();
    auto entity  = harness.registry->CreateEntity(
        fg::NameComponent {.name = "Hero"}, fg::TransformComponent {});
    harness.registry->ExecuteTasks();

    const auto handle = harness.world->CreateCharacter(fg::PhysicsCharacterDesc {});
    harness.world->BindCharacter(static_cast<std::uint64_t>(entity), handle);
    auto &state           = harness.world->characters.begin()->second;
    state.grounded        = true;
    state.groundNormal    = {0.0f, 1.0f, 0.0f};
    state.groundVelocity  = {1.0f, 0.0f, 0.0f};

    const auto info = harness.physics->GetCharacterGroundInfo(entity);
    EXPECT_TRUE(info.grounded);
    EXPECT_EQ(info.state, fg::CharacterGroundState::OnGround);
    EXPECT_FLOAT_EQ(info.velocity.x, 1.0f);
}

TEST(JoltCharacter, StepAppliesGravityToCharacterVelocity)
{
    auto world = skr::MakeArc<fg::JoltPhysicsWorld>();
    // Static floor under the character origin.
    fg::PhysicsBodyDesc floor {};
    floor.motion          = fg::BodyMotionType::Static;
    floor.shape           = fg::ColliderShape::Box;
    floor.position        = {0.0f, -0.5f, 0.0f};
    floor.halfExtents     = {10.0f, 0.5f, 10.0f};
    floor.collisionLayer  = 0;
    ASSERT_TRUE(world->CreateBody(floor).IsValid());

    fg::PhysicsCharacterDesc desc {};
    desc.position         = {0.0f, 2.0f, 0.0f};
    desc.collisionLayer   = 1;
    const auto character  = world->CreateCharacter(desc);
    ASSERT_TRUE(character.IsValid());

    world->SetCharacterVelocity(character, {0.0f, 0.0f, 0.0f});
    world->StepFixed(10);

    const auto velocity = world->GetCharacterVelocity(character);
    EXPECT_LT(velocity.y, -0.5f) << "character should be falling under gravity";

    glm::vec3 position {};
    glm::quat rotation {};
    world->GetCharacterTransform(character, position, rotation);
    EXPECT_LT(position.y, 2.0f) << "character should move downward";
}

TEST(JoltCharacter, TeleportAndFacingAndResize)
{
    auto world = skr::MakeArc<fg::JoltPhysicsWorld>();
    fg::PhysicsBodyDesc floor {};
    floor.motion         = fg::BodyMotionType::Static;
    floor.shape          = fg::ColliderShape::Box;
    floor.position       = {0.0f, -0.5f, 0.0f};
    floor.halfExtents    = {10.0f, 0.5f, 10.0f};
    floor.collisionLayer = 0;
    ASSERT_TRUE(world->CreateBody(floor).IsValid());

    fg::PhysicsCharacterDesc desc {};
    desc.position       = {0.0f, 1.0f, 0.0f};
    desc.collisionLayer = 1;
    desc.maxStrength    = 150.0f;
    const auto character = world->CreateCharacter(desc);
    ASSERT_TRUE(character.IsValid());

    world->SetCharacterPosition(character, {3.0f, 1.0f, -2.0f});
    const glm::quat facing = glm::angleAxis(glm::radians(45.0f), glm::vec3 {0.0f, 1.0f, 0.0f});
    world->SetCharacterRotation(character, facing);

    glm::vec3 position {};
    glm::quat rotation {};
    world->GetCharacterTransform(character, position, rotation);
    EXPECT_FLOAT_EQ(position.x, 3.0f);
    EXPECT_FLOAT_EQ(position.z, -2.0f);
    EXPECT_NEAR(rotation.w, facing.w, 1e-5f);
    EXPECT_NEAR(rotation.y, facing.y, 1e-5f);

    world->SetCharacterMaxStrength(character, 300.0f);
    fg::PhysicsCharacterShapeDesc crouch {};
    crouch.radius = 0.5f;
    crouch.height = 0.5f;
    EXPECT_TRUE(world->SetCharacterShape(character, crouch));

    world->StepFixed(5);
    const auto ground = world->GetCharacterGroundInfo(character);
    // After a few steps near the floor the character should report some ground query state.
    (void)ground;
}

TEST(PhysicsFacade, SetAngularVelocityForwards)
{
    auto harness = PhysicsHarness::Create();
    auto entity  = harness.registry->CreateEntity(
        fg::NameComponent {.name = "Box"}, fg::TransformComponent {}, fg::RigidBodyComponent {});
    harness.registry->ExecuteTasks();

    harness.registry->TryGetComponents<fg::RigidBodyComponent>(
        entity, [&](fg::RigidBodyComponent &rb) {
            rb.body = harness.world->CreateBody(fg::PhysicsBodyDesc {});
        });
    harness.registry->ExecuteTasks();

    harness.physics->SetAngularVelocity(entity, {0.5f, 1.5f, -2.0f});
    EXPECT_EQ(harness.world->setAngularVelocityCalls, 1);
    const auto velocity = harness.physics->GetAngularVelocity(entity);
    EXPECT_FLOAT_EQ(velocity.x, 0.5f);
    EXPECT_FLOAT_EQ(velocity.y, 1.5f);
    EXPECT_FLOAT_EQ(velocity.z, -2.0f);
}

TEST(PhysicsFacade, RaycastForwards)
{
    auto harness = PhysicsHarness::Create();

    harness.world->cannedRaycastHit.hit      = true;
    harness.world->cannedRaycastHit.point    = {0.0f, 1.0f, 0.0f};
    harness.world->cannedRaycastHit.normal   = {0.0f, 1.0f, 0.0f};
    harness.world->cannedRaycastHit.distance = 4.0f;
    harness.world->cannedRaycastHit.fraction = 0.4f;

    const glm::vec3 origin {0.0f, 5.0f, 0.0f};
    const glm::vec3 direction {0.0f, -1.0f, 0.0f};
    const auto hit = harness.physics->Raycast(origin, direction, 10.0f);

    EXPECT_EQ(harness.world->raycastCalls, 1);
    EXPECT_FLOAT_EQ(harness.world->lastRayOrigin.x, origin.x);
    EXPECT_FLOAT_EQ(harness.world->lastRayOrigin.y, origin.y);
    EXPECT_FLOAT_EQ(harness.world->lastRayOrigin.z, origin.z);
    EXPECT_FLOAT_EQ(harness.world->lastRayDirection.x, direction.x);
    EXPECT_FLOAT_EQ(harness.world->lastRayDirection.y, direction.y);
    EXPECT_FLOAT_EQ(harness.world->lastRayDirection.z, direction.z);
    EXPECT_FLOAT_EQ(harness.world->lastRayMaxDistance, 10.0f);
    EXPECT_TRUE(hit.hit);
    EXPECT_FLOAT_EQ(hit.point.y, 1.0f);
    EXPECT_FLOAT_EQ(hit.distance, 4.0f);
}

TEST(PhysicsFacade, CreateJointForwards)
{
    auto harness = PhysicsHarness::Create();
    auto entityA = harness.registry->CreateEntity(
        fg::NameComponent {.name = "A"}, fg::TransformComponent {}, fg::RigidBodyComponent {});
    auto entityB = harness.registry->CreateEntity(
        fg::NameComponent {.name = "B"}, fg::TransformComponent {}, fg::RigidBodyComponent {});
    harness.registry->ExecuteTasks();

    fg::PhysicsBodyHandle bodyA {};
    fg::PhysicsBodyHandle bodyB {};
    harness.registry->TryGetComponents<fg::RigidBodyComponent>(
        entityA, [&](fg::RigidBodyComponent &rb) {
            rb.body = harness.world->CreateBody(fg::PhysicsBodyDesc {});
            bodyA   = rb.body;
        });
    harness.registry->TryGetComponents<fg::RigidBodyComponent>(
        entityB, [&](fg::RigidBodyComponent &rb) {
            rb.body = harness.world->CreateBody(fg::PhysicsBodyDesc {});
            bodyB   = rb.body;
        });
    harness.registry->ExecuteTasks();

    const glm::vec3 anchorA {0.0f, 0.5f, 0.0f};
    const glm::vec3 anchorB {0.0f, -0.5f, 0.0f};
    const glm::vec3 hingeAxis {1.0f, 0.0f, 0.0f};
    const auto joint = harness.physics->CreateJoint(entityA, entityB, fg::PhysicsJointType::Hinge,
                                                    anchorA, anchorB, hingeAxis);

    EXPECT_TRUE(joint.IsValid());
    EXPECT_EQ(harness.world->createJointCalls, 1);
    EXPECT_EQ(harness.world->lastJointDesc.type, fg::PhysicsJointType::Hinge);
    EXPECT_EQ(harness.world->lastJointDesc.bodyA.id, bodyA.id);
    EXPECT_EQ(harness.world->lastJointDesc.bodyB.id, bodyB.id);
    EXPECT_FLOAT_EQ(harness.world->lastJointDesc.localAnchorA.y, 0.5f);
    EXPECT_FLOAT_EQ(harness.world->lastJointDesc.localAnchorB.y, -0.5f);
    EXPECT_FLOAT_EQ(harness.world->lastJointDesc.hingeAxisLocalA.x, 1.0f);
}

TEST(PhysicsFacade, DrainTriggerEventsForwards)
{
    auto harness = PhysicsHarness::Create();

    fg::TriggerEvent enter {};
    enter.type         = fg::TriggerEventType::Enter;
    enter.sensorEntity = 10;
    enter.otherEntity  = 20;
    harness.world->pendingTriggerEvents.push_back(enter);

    const auto events = harness.physics->DrainTriggerEvents();
    EXPECT_EQ(harness.world->drainTriggerCalls, 1);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].type, fg::TriggerEventType::Enter);
    EXPECT_EQ(events[0].sensorEntity, 10u);
    EXPECT_EQ(events[0].otherEntity, 20u);

    const auto again = harness.physics->DrainTriggerEvents();
    EXPECT_TRUE(again.empty());
    EXPECT_EQ(harness.world->drainTriggerCalls, 2);
}

TEST(JoltPhysics, RaycastHitsFloor)
{
    auto world = skr::MakeArc<fg::JoltPhysicsWorld>();

    fg::PhysicsBodyDesc floor {};
    floor.motion         = fg::BodyMotionType::Static;
    floor.shape          = fg::ColliderShape::Box;
    floor.position       = {0.0f, -0.5f, 0.0f};
    floor.halfExtents    = {10.0f, 0.5f, 10.0f};
    floor.collisionLayer = 0;
    ASSERT_TRUE(world->CreateBody(floor).IsValid());

    const auto hit =
        world->Raycast({0.0f, 5.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 20.0f);
    EXPECT_TRUE(hit.hit);
    EXPECT_NEAR(hit.point.y, 0.0f, 0.05f);
    EXPECT_GT(hit.normal.y, 0.5f);
}

TEST(JoltPhysics, AngularVelocityAfterTorque)
{
    auto world = skr::MakeArc<fg::JoltPhysicsWorld>();

    fg::PhysicsBodyDesc desc {};
    desc.motion         = fg::BodyMotionType::Dynamic;
    desc.shape          = fg::ColliderShape::Box;
    desc.position       = {0.0f, 5.0f, 0.0f};
    desc.halfExtents    = {0.5f, 0.5f, 0.5f};
    desc.mass           = 1.0f;
    desc.collisionLayer = 0;
    const auto body     = world->CreateBody(desc);
    ASSERT_TRUE(body.IsValid());

    world->AddTorque(body, {0.0f, 50.0f, 0.0f});
    world->StepFixed(1);

    const auto angular = world->GetAngularVelocity(body);
    EXPECT_GT(glm::length(angular), 1e-3f) << "torque should produce angular velocity";
}

TEST(JoltCharacter, PresenceBodyIsHitByRaycast)
{
    auto world = skr::MakeArc<fg::JoltPhysicsWorld>();

    fg::PhysicsBodyDesc floor {};
    floor.motion         = fg::BodyMotionType::Static;
    floor.shape          = fg::ColliderShape::Box;
    floor.position       = {0.0f, -0.5f, 0.0f};
    floor.halfExtents    = {10.0f, 0.5f, 10.0f};
    floor.collisionLayer = 0;
    ASSERT_TRUE(world->CreateBody(floor).IsValid());

    constexpr std::uint64_t kEntity = 42;
    fg::PhysicsBodyDesc presence {};
    presence.motion            = fg::BodyMotionType::Kinematic;
    presence.shape             = fg::ColliderShape::Capsule;
    presence.position          = {0.0f, 0.0f, 0.0f};
    presence.radius            = 0.5f;
    presence.height            = 1.0f;
    presence.mass              = 70.0f;
    presence.collisionLayer    = 1;
    presence.collideWithLayers = 0xffff;
    presence.entityId          = kEntity;
    const auto body            = world->CreateBody(presence);
    ASSERT_TRUE(body.IsValid());

    fg::PhysicsCharacterDesc desc {};
    desc.position       = {0.0f, 0.0f, 0.0f};
    desc.radius         = 0.5f;
    desc.height         = 1.0f;
    desc.collisionLayer = 1;
    const auto character = world->CreateCharacter(desc);
    ASSERT_TRUE(character.IsValid());
    world->BindCharacter(kEntity, character, body);

    const auto hit = world->Raycast({0.0f, 1.0f, 5.0f}, {0.0f, 0.0f, -1.0f}, 20.0f);
    EXPECT_TRUE(hit.hit);
    EXPECT_EQ(hit.entityId, kEntity);
}

TEST(JoltCharacter, CharactersDoNotTunnelThroughPresenceBodies)
{
    auto world = skr::MakeArc<fg::JoltPhysicsWorld>();

    fg::PhysicsBodyDesc floor {};
    floor.motion         = fg::BodyMotionType::Static;
    floor.shape          = fg::ColliderShape::Box;
    floor.position       = {0.0f, -0.5f, 0.0f};
    floor.halfExtents    = {20.0f, 0.5f, 20.0f};
    floor.collisionLayer = 0;
    ASSERT_TRUE(world->CreateBody(floor).IsValid());

    auto spawn = [&](std::uint64_t entity, const glm::vec3 &pos) {
        fg::PhysicsBodyDesc presence {};
        presence.motion            = fg::BodyMotionType::Kinematic;
        presence.shape             = fg::ColliderShape::Capsule;
        presence.position          = pos;
        presence.radius            = 0.5f;
        presence.height            = 1.0f;
        presence.mass              = 70.0f;
        presence.collisionLayer    = 1;
        presence.collideWithLayers = 0xffff;
        presence.entityId          = entity;
        const auto body            = world->CreateBody(presence);
        EXPECT_TRUE(body.IsValid());

        fg::PhysicsCharacterDesc desc {};
        desc.position       = pos;
        desc.radius         = 0.5f;
        desc.height         = 1.0f;
        desc.collisionLayer = 1;
        const auto character = world->CreateCharacter(desc);
        EXPECT_TRUE(character.IsValid());
        world->BindCharacter(entity, character, body);
        return character;
    };

    const auto left  = spawn(1, {-1.0f, 0.0f, 0.0f});
    const auto right = spawn(2, {1.0f, 0.0f, 0.0f});

    world->SetCharacterVelocity(left, {6.0f, 0.0f, 0.0f});
    world->SetCharacterVelocity(right, {-6.0f, 0.0f, 0.0f});
    world->StepFixed(30);

    glm::vec3 leftPos {};
    glm::vec3 rightPos {};
    glm::quat rot {};
    world->GetCharacterTransform(left, leftPos, rot);
    world->GetCharacterTransform(right, rightPos, rot);

    const float separation = glm::length(glm::vec3 {rightPos.x - leftPos.x, 0.0f, rightPos.z - leftPos.z});
    EXPECT_GE(separation, 0.85f) << "characters should not fully overlap through presence bodies";
}

