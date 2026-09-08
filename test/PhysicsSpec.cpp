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
            joints.clear();
            pendingTriggerEvents.clear();
            pendingContactEvents.clear();
            nextBody  = 1;
            nextJoint = 1;
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

        [[nodiscard]] fg::RaycastHit SphereCast(const glm::vec3 &origin, const glm::vec3 &direction,
                                                float radius, float maxDistance,
                                                const fg::QueryFilter &filter) const override
        {
            lastSphereOrigin      = origin;
            lastSphereDirection   = direction;
            lastSphereRadius      = radius;
            lastSphereMaxDistance = maxDistance;
            lastSphereFilter      = filter;
            ++sphereCastCalls;
            return cannedSphereCastHit;
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

        std::unordered_map<std::uint32_t, BodyState> bodies;
        std::unordered_map<std::uint32_t, fg::PhysicsJointDesc> joints;
        std::uint32_t nextBody  = 1;
        std::uint32_t nextJoint = 1;
        glm::vec3 gravity {0.0f, -9.81f, 0.0f};

        mutable glm::vec3 lastRayOrigin {0.0f};
        mutable glm::vec3 lastRayDirection {0.0f};
        mutable float lastRayMaxDistance = 0.0f;
        mutable fg::QueryFilter lastRayFilter {};
        mutable int raycastCalls = 0;
        fg::RaycastHit cannedRaycastHit {};

        mutable glm::vec3 lastSphereOrigin {0.0f};
        mutable glm::vec3 lastSphereDirection {0.0f};
        mutable float lastSphereRadius      = 0.0f;
        mutable float lastSphereMaxDistance = 0.0f;
        mutable fg::QueryFilter lastSphereFilter {};
        mutable int sphereCastCalls = 0;
        fg::RaycastHit cannedSphereCastHit {};

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

        fr::Entity CreateDynamicCharacter(const char *name,
                                          const glm::vec3 &position = {},
                                          float radius = 0.5f, float height = 1.0f)
        {
            auto entity = registry->CreateEntity(
                fg::NameComponent {.name = name},
                fg::TransformComponent {.position = position},
                fg::RigidBodyComponent {.motion = fg::BodyMotionType::Dynamic,
                                        .shape  = fg::ColliderShape::Capsule,
                                        .radius = radius,
                                        .height = height,
                                        .mass   = 70.0f});
            registry->ExecuteTasks();

            registry->TryGetComponents<fg::RigidBodyComponent>(
                entity, [&](fg::RigidBodyComponent &rb) {
                    fg::PhysicsBodyDesc desc {};
                    desc.motion         = fg::BodyMotionType::Dynamic;
                    desc.shape          = fg::ColliderShape::Capsule;
                    desc.position       = position;
                    desc.radius         = radius;
                    desc.height         = height;
                    desc.mass           = 70.0f;
                    desc.collisionLayer = 1;
                    rb.body             = world->CreateBody(desc);
                    world->SetTransform(rb.body, position, {1.0f, 0.0f, 0.0f, 0.0f});
                });
            registry->ExecuteTasks();
            return entity;
        }
    };

    fg::PhysicsBodyDesc MakeFloorDesc(float y = -0.5f, float halfY = 0.5f,
                                      const glm::vec3 &halfXZ = {10.0f, 0.5f, 10.0f})
    {
        fg::PhysicsBodyDesc floor {};
        floor.motion         = fg::BodyMotionType::Static;
        floor.shape          = fg::ColliderShape::Box;
        floor.position       = {0.0f, y, 0.0f};
        floor.halfExtents    = {halfXZ.x, halfY, halfXZ.z};
        floor.collisionLayer = 0;
        return floor;
    }

    fg::PhysicsBodyDesc MakeDynamicCapsule(const glm::vec3 &position, std::uint64_t entityId = 0)
    {
        fg::PhysicsBodyDesc desc {};
        desc.motion            = fg::BodyMotionType::Dynamic;
        desc.shape             = fg::ColliderShape::Capsule;
        desc.position          = position;
        desc.radius            = 0.5f;
        desc.height            = 1.0f;
        desc.mass              = 70.0f;
        desc.collisionLayer    = 1;
        desc.collideWithLayers = 0xffff;
        desc.entityId          = entityId;
        return desc;
    }
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
    EXPECT_EQ(harness.world->setVelocityCalls, 0);
    EXPECT_FALSE(harness.physics->IsCharacterGrounded(entity));
}

TEST(PhysicsFacade, MoveCharacterForwardsVelocity)
{
    auto harness = PhysicsHarness::Create();
    auto entity  = harness.CreateDynamicCharacter("Hero");

    harness.physics->MoveCharacter(entity, {2.0f, 0.0f, -1.0f});

    EXPECT_EQ(harness.world->setVelocityCalls, 1);
    const auto linear = harness.physics->GetLinearVelocity(entity);
    EXPECT_FLOAT_EQ(linear.x, 2.0f);
    EXPECT_FLOAT_EQ(linear.y, 0.0f);
    EXPECT_FLOAT_EQ(linear.z, -1.0f);
    const auto characterVelocity = harness.physics->GetCharacterVelocity(entity);
    EXPECT_FLOAT_EQ(characterVelocity.x, 2.0f);
    EXPECT_FLOAT_EQ(characterVelocity.y, 0.0f);
    EXPECT_FLOAT_EQ(characterVelocity.z, -1.0f);
}

TEST(PhysicsFacade, TeleportCharacterUpdatesTransformAndWorld)
{
    auto harness = PhysicsHarness::Create();
    auto entity  = harness.CreateDynamicCharacter("Hero", {1.0f, 0.0f, 0.0f});
    const int transformsBefore = harness.world->setTransformCalls;

    const glm::vec3 dest {9.0f, 2.0f, -3.0f};
    harness.physics->TeleportCharacter(entity, dest);

    EXPECT_GT(harness.world->setTransformCalls, transformsBefore);
    harness.registry->TryGetComponents<fg::TransformComponent>(
        entity, [&](fg::TransformComponent &transform) {
            EXPECT_FLOAT_EQ(transform.position.x, dest.x);
            EXPECT_FLOAT_EQ(transform.position.y, dest.y);
            EXPECT_FLOAT_EQ(transform.position.z, dest.z);
        });

    const auto velocity = harness.physics->GetLinearVelocity(entity);
    EXPECT_FLOAT_EQ(velocity.x, 0.0f);
    EXPECT_FLOAT_EQ(velocity.y, 0.0f);
    EXPECT_FLOAT_EQ(velocity.z, 0.0f);
}

TEST(PhysicsFacade, SetCharacterFacingUpdatesTransformAndWorld)
{
    auto harness = PhysicsHarness::Create();
    auto entity  = harness.CreateDynamicCharacter("Hero");
    const int transformsBefore = harness.world->setTransformCalls;

    const glm::quat facing = glm::angleAxis(glm::radians(90.0f), glm::vec3 {0.0f, 1.0f, 0.0f});
    harness.physics->SetCharacterFacing(entity, facing);

    EXPECT_GT(harness.world->setTransformCalls, transformsBefore);
    harness.registry->TryGetComponents<fg::TransformComponent>(
        entity, [&](fg::TransformComponent &transform) {
            EXPECT_NEAR(transform.rotation.w, facing.w, 1e-5f);
            EXPECT_NEAR(transform.rotation.x, facing.x, 1e-5f);
            EXPECT_NEAR(transform.rotation.y, facing.y, 1e-5f);
            EXPECT_NEAR(transform.rotation.z, facing.z, 1e-5f);
        });
}

TEST(PhysicsFacade, CharacterShapeUpdatesRigidBody)
{
    auto harness = PhysicsHarness::Create();
    auto entity  = harness.CreateDynamicCharacter("Hero");

    EXPECT_TRUE(harness.physics->SetCharacterShape(entity, 0.4f, 0.6f, {0.0f, 0.1f, 0.0f}));
    harness.registry->TryGetComponents<fg::RigidBodyComponent>(
        entity, [&](fg::RigidBodyComponent &rb) {
            EXPECT_EQ(rb.shape, fg::ColliderShape::Capsule);
            EXPECT_FLOAT_EQ(rb.radius, 0.4f);
            EXPECT_FLOAT_EQ(rb.height, 0.6f);
            EXPECT_FLOAT_EQ(rb.centerOffset.y, 0.1f);
        });
}

TEST(PhysicsFacade, GetCharacterGroundInfo)
{
    auto harness = PhysicsHarness::Create();
    auto entity  = harness.CreateDynamicCharacter("Hero", {0.0f, 1.0f, 0.0f});

    harness.world->cannedSphereCastHit.hit    = true;
    harness.world->cannedSphereCastHit.point  = {0.0f, 0.0f, 0.0f};
    harness.world->cannedSphereCastHit.normal = {0.0f, 1.0f, 0.0f};
    harness.world->cannedSphereCastHit.body   = {.id = 99};

    harness.physics->MoveCharacter(entity, {1.0f, 0.0f, 0.0f});

    const auto info = harness.physics->GetCharacterGroundInfo(entity);
    EXPECT_TRUE(info.grounded);
    EXPECT_EQ(info.state, fg::CharacterGroundState::OnGround);
    EXPECT_FLOAT_EQ(info.velocity.x, 1.0f);
    EXPECT_EQ(info.groundBody.id, 99u);
    EXPECT_GT(harness.world->sphereCastCalls, 0);
}

TEST(PhysicsFacade, AddImpulseForwardsToCharacterBody)
{
    auto harness = PhysicsHarness::Create();
    auto entity  = harness.CreateDynamicCharacter("Hero");

    harness.physics->AddImpulse(entity, {10.0f, 0.0f, 0.0f});
    EXPECT_EQ(harness.world->impulseCalls, 1);

    fg::PhysicsBodyHandle body {};
    harness.registry->TryGetComponents<fg::RigidBodyComponent>(
        entity, [&](fg::RigidBodyComponent &rb) { body = rb.body; });
    ASSERT_TRUE(body.IsValid());
    EXPECT_FLOAT_EQ(harness.world->bodies[body.id].impulse.x, 10.0f);
}

TEST(JoltPhysics, StepAppliesGravityToDynamicCharacter)
{
    auto world = skr::MakeArc<fg::JoltPhysicsWorld>();
    ASSERT_TRUE(world->CreateBody(MakeFloorDesc()).IsValid());

    const auto body = world->CreateBody(MakeDynamicCapsule({0.0f, 2.0f, 0.0f}));
    ASSERT_TRUE(body.IsValid());

    world->SetLinearVelocity(body, {0.0f, 0.0f, 0.0f});
    world->StepFixed(10);

    const auto velocity = world->GetLinearVelocity(body);
    EXPECT_LT(velocity.y, -0.5f) << "dynamic character body should fall under gravity";

    glm::vec3 position {};
    glm::quat rotation {};
    world->GetTransform(body, position, rotation);
    EXPECT_LT(position.y, 2.0f) << "dynamic character body should move downward";
}

TEST(JoltPhysics, TeleportAndFacingOnDynamicCharacter)
{
    auto world = skr::MakeArc<fg::JoltPhysicsWorld>();
    ASSERT_TRUE(world->CreateBody(MakeFloorDesc()).IsValid());

    const auto body = world->CreateBody(MakeDynamicCapsule({0.0f, 1.0f, 0.0f}));
    ASSERT_TRUE(body.IsValid());

    const glm::quat facing = glm::angleAxis(glm::radians(45.0f), glm::vec3 {0.0f, 1.0f, 0.0f});
    world->SetTransform(body, {3.0f, 1.0f, -2.0f}, facing);
    world->SetLinearVelocity(body, {});
    world->SetAngularVelocity(body, {});

    glm::vec3 position {};
    glm::quat rotation {};
    world->GetTransform(body, position, rotation);
    EXPECT_FLOAT_EQ(position.x, 3.0f);
    EXPECT_FLOAT_EQ(position.z, -2.0f);
    EXPECT_NEAR(rotation.w, facing.w, 1e-5f);
    EXPECT_NEAR(rotation.y, facing.y, 1e-5f);

    world->StepFixed(5);
    // Pose remains readable after simulation ticks.
    world->GetTransform(body, position, rotation);
    EXPECT_NEAR(position.x, 3.0f, 0.25f);
}

TEST(JoltPhysics, AddImpulseChangesDynamicCharacterVelocity)
{
    auto world = skr::MakeArc<fg::JoltPhysicsWorld>();
    ASSERT_TRUE(world->CreateBody(MakeFloorDesc()).IsValid());

    const auto body = world->CreateBody(MakeDynamicCapsule({0.0f, 1.0f, 0.0f}));
    ASSERT_TRUE(body.IsValid());

    world->SetLinearVelocity(body, {});
    world->AddImpulse(body, {350.0f, 0.0f, 0.0f});
    world->StepFixed(1);

    const auto velocity = world->GetLinearVelocity(body);
    EXPECT_GT(velocity.x, 1.0f) << "AddImpulse should change dynamic character body velocity";
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

    ASSERT_TRUE(world->CreateBody(MakeFloorDesc()).IsValid());

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

TEST(JoltPhysics, DynamicCapsuleIsHitByRaycast)
{
    auto world = skr::MakeArc<fg::JoltPhysicsWorld>();

    ASSERT_TRUE(world->CreateBody(MakeFloorDesc()).IsValid());

    constexpr std::uint64_t kEntity = 42;
    const auto body = world->CreateBody(MakeDynamicCapsule({0.0f, 1.0f, 0.0f}, kEntity));
    ASSERT_TRUE(body.IsValid());

    const auto hit = world->Raycast({0.0f, 1.0f, 5.0f}, {0.0f, 0.0f, -1.0f}, 20.0f);
    EXPECT_TRUE(hit.hit);
    EXPECT_EQ(hit.entityId, kEntity);
}

TEST(JoltPhysics, DynamicCapsulesDoNotTunnelThroughEachOther)
{
    auto world = skr::MakeArc<fg::JoltPhysicsWorld>();

    fg::PhysicsBodyDesc floor = MakeFloorDesc(-0.5f, 0.5f, {20.0f, 0.5f, 20.0f});
    ASSERT_TRUE(world->CreateBody(floor).IsValid());

    const auto left  = world->CreateBody(MakeDynamicCapsule({-1.0f, 1.0f, 0.0f}, 1));
    const auto right = world->CreateBody(MakeDynamicCapsule({1.0f, 1.0f, 0.0f}, 2));
    ASSERT_TRUE(left.IsValid());
    ASSERT_TRUE(right.IsValid());

    world->SetLinearVelocity(left, {6.0f, 0.0f, 0.0f});
    world->SetLinearVelocity(right, {-6.0f, 0.0f, 0.0f});
    world->StepFixed(30);

    glm::vec3 leftPos {};
    glm::vec3 rightPos {};
    glm::quat rot {};
    world->GetTransform(left, leftPos, rot);
    world->GetTransform(right, rightPos, rot);

    const float separation =
        glm::length(glm::vec3 {rightPos.x - leftPos.x, 0.0f, rightPos.z - leftPos.z});
    EXPECT_GE(separation, 0.85f) << "dynamic capsules should not fully overlap";
}
