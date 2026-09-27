#include <Frigga/Physics/JoltPhysicsWorld.hpp>

#include <gtest/gtest.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace
{
    constexpr float kDt = 1.0f / 60.0f;
    /// Jolt BodyCreationSettings default (dv/dt = -c * v), applied every fixed step.
    constexpr float kDefaultDamping = 0.05f;

    constexpr std::uint8_t kFloorLayer   = 0;
    constexpr std::uint8_t kDynamicLayer = 1;
    constexpr std::uint8_t kSensorLayer  = 2;
    constexpr std::uint8_t kGhostLayer   = 3;

    fg::PhysicsBodyDesc MakeFloor(std::uint64_t entityId = 0)
    {
        fg::PhysicsBodyDesc floor{};
        floor.motion         = fg::BodyMotionType::Static;
        floor.shape          = fg::ColliderShape::Box;
        floor.position       = {0.0f, -0.5f, 0.0f};
        floor.halfExtents    = {20.0f, 0.5f, 20.0f};
        floor.collisionLayer = kFloorLayer;
        floor.entityId       = entityId;
        return floor;
    }

    /// Unit cube (1 m side). Inertia about any axis = m * (1^2 + 1^2) / 12 = m / 6.
    fg::PhysicsBodyDesc MakeBox(const glm::vec3 &position, float mass = 2.0f,
                                std::uint64_t entityId = 0)
    {
        fg::PhysicsBodyDesc desc{};
        desc.motion         = fg::BodyMotionType::Dynamic;
        desc.shape          = fg::ColliderShape::Box;
        desc.position       = position;
        desc.halfExtents    = {0.5f, 0.5f, 0.5f};
        desc.mass           = mass;
        desc.collisionLayer = kDynamicLayer;
        desc.entityId       = entityId;
        return desc;
    }

    fg::PhysicsBodyDesc MakeSphere(const glm::vec3 &position, float radius,
                                   std::uint64_t entityId = 0)
    {
        fg::PhysicsBodyDesc desc{};
        desc.motion         = fg::BodyMotionType::Dynamic;
        desc.shape          = fg::ColliderShape::Sphere;
        desc.position       = position;
        desc.radius         = radius;
        desc.mass           = 1.0f;
        desc.collisionLayer = kDynamicLayer;
        desc.entityId       = entityId;
        return desc;
    }

    glm::vec3 PositionOf(const fg::JoltPhysicsWorld &world, fg::PhysicsBodyHandle body)
    {
        glm::vec3 position{};
        glm::quat rotation{};
        world.GetTransform(body, position, rotation);
        return position;
    }

    /// Velocity after @p steps fixed ticks of constant acceleration with Jolt linear damping.
    float DampedVelocity(float acceleration, int steps)
    {
        float v = 0.0f;
        for(int i = 0; i < steps; ++i)
        {
            v = (v + acceleration * kDt) * (1.0f - kDefaultDamping * kDt);
        }
        return v;
    }

    std::size_t CountPair(const std::vector<fg::PhysicsContactEvent> &events, std::uint64_t a,
                          std::uint64_t b)
    {
        return static_cast<std::size_t>(
            std::count_if(events.begin(), events.end(), [&](const fg::PhysicsContactEvent &e) {
                return e.entityA == std::min(a, b) && e.entityB == std::max(a, b);
            }));
    }
} // namespace

// ---------------------------------------------------------------------------------------------
// Forces and impulses
// ---------------------------------------------------------------------------------------------

TEST(JoltPhysicsForces, AddImpulseChangesVelocityByImpulseOverMass)
{
    fg::JoltPhysicsWorld world;
    world.SetGravity({0.0f, 0.0f, 0.0f});
    const auto body = world.CreateBody(MakeBox({0.0f, 5.0f, 0.0f}, 2.0f));
    ASSERT_TRUE(body.IsValid());

    world.AddImpulse(body, {10.0f, 0.0f, -4.0f});

    // Impulses act on velocity immediately (no step needed): dv = J / m.
    const auto velocity = world.GetLinearVelocity(body);
    EXPECT_NEAR(velocity.x, 5.0f, 1e-4f);
    EXPECT_NEAR(velocity.y, 0.0f, 1e-4f);
    EXPECT_NEAR(velocity.z, -2.0f, 1e-4f);
}

TEST(JoltPhysicsForces, ImpulseVelocityIntegratesIntoPosition)
{
    fg::JoltPhysicsWorld world;
    world.SetGravity({0.0f, 0.0f, 0.0f});
    const auto body = world.CreateBody(MakeBox({0.0f, 5.0f, 0.0f}, 2.0f));
    ASSERT_TRUE(body.IsValid());

    world.AddImpulse(body, {6.0f, 0.0f, 0.0f}); // 3 m/s
    world.StepFixed(60);                        // 1 s

    const auto position = PositionOf(world, body);
    EXPECT_NEAR(position.x, 3.0f, 3.0f * 0.03f) << "~3 m/s for 1 s, minus linear damping";
    EXPECT_NEAR(position.y, 5.0f, 1e-3f);
    EXPECT_NEAR(position.z, 0.0f, 1e-3f);
}

TEST(JoltPhysicsForces, AddForceAcceleratesForOneStepThenIsCleared)
{
    fg::JoltPhysicsWorld world;
    world.SetGravity({0.0f, 0.0f, 0.0f});
    const auto body = world.CreateBody(MakeBox({0.0f, 5.0f, 0.0f}, 2.0f));
    ASSERT_TRUE(body.IsValid());

    world.AddForce(body, {120.0f, 0.0f, 0.0f}); // a = 60 m/s^2
    EXPECT_NEAR(world.GetLinearVelocity(body).x, 0.0f, 1e-6f)
        << "forces are only integrated during a step";

    world.StepFixed(1);
    const float afterForce = world.GetLinearVelocity(body).x;
    EXPECT_NEAR(afterForce, 60.0f * kDt, 60.0f * kDt * 0.01f) << "dv = F / m * dt";

    world.StepFixed(1);
    const float afterNextStep = world.GetLinearVelocity(body).x;
    EXPECT_NEAR(afterNextStep, afterForce, afterForce * 0.01f)
        << "force must not persist into the next step";
}

TEST(JoltPhysicsForces, AddAngularImpulseSpinsAroundImpulseAxis)
{
    fg::JoltPhysicsWorld world;
    world.SetGravity({0.0f, 0.0f, 0.0f});
    const auto body = world.CreateBody(MakeBox({0.0f, 5.0f, 0.0f}, 2.0f));
    ASSERT_TRUE(body.IsValid());

    world.AddAngularImpulse(body, {0.0f, 1.0f, 0.0f});

    // w = L / I with I = m / 6 = 1/3 for a 2 kg unit cube.
    const auto angular = world.GetAngularVelocity(body);
    EXPECT_NEAR(angular.y, 3.0f, 3.0f * 0.02f);
    EXPECT_NEAR(angular.x, 0.0f, 1e-4f);
    EXPECT_NEAR(angular.z, 0.0f, 1e-4f);
}

TEST(JoltPhysicsForces, AddTorqueProducesAngularVelocityAlongTorque)
{
    fg::JoltPhysicsWorld world;
    world.SetGravity({0.0f, 0.0f, 0.0f});
    const auto body = world.CreateBody(MakeBox({0.0f, 5.0f, 0.0f}, 2.0f));
    ASSERT_TRUE(body.IsValid());

    world.AddTorque(body, {0.0f, 0.0f, 6.0f}); // alpha = tau / I = 18 rad/s^2
    world.StepFixed(1);

    const auto angular = world.GetAngularVelocity(body);
    EXPECT_NEAR(angular.z, 18.0f * kDt, 18.0f * kDt * 0.02f);
    EXPECT_NEAR(angular.x, 0.0f, 1e-4f);
    EXPECT_NEAR(angular.y, 0.0f, 1e-4f);
}

TEST(JoltPhysicsForces, LockedRotationAxisIgnoresAngularImpulse)
{
    fg::JoltPhysicsWorld world;
    world.SetGravity({0.0f, 0.0f, 0.0f});
    auto desc          = MakeBox({0.0f, 5.0f, 0.0f}, 2.0f);
    desc.lockRotationY = true;
    const auto body    = world.CreateBody(desc);
    ASSERT_TRUE(body.IsValid());

    world.AddAngularImpulse(body, {0.0f, 1.0f, 0.0f});
    world.StepFixed(1);

    EXPECT_NEAR(world.GetAngularVelocity(body).y, 0.0f, 1e-4f);

    world.AddAngularImpulse(body, {1.0f, 0.0f, 0.0f});
    EXPECT_GT(world.GetAngularVelocity(body).x, 1.0f) << "unlocked axes still rotate";
}

TEST(JoltPhysicsForces, StaticAndKinematicBodiesIgnoreForcesAndImpulses)
{
    fg::JoltPhysicsWorld world;

    const auto floor = world.CreateBody(MakeFloor());
    ASSERT_TRUE(floor.IsValid());

    auto kinematicDesc   = MakeBox({0.0f, 5.0f, 0.0f});
    kinematicDesc.motion = fg::BodyMotionType::Kinematic;
    const auto kinematic = world.CreateBody(kinematicDesc);
    ASSERT_TRUE(kinematic.IsValid());

    for(const auto body: {floor, kinematic})
    {
        world.AddImpulse(body, {100.0f, 0.0f, 0.0f});
        world.AddForce(body, {100.0f, 0.0f, 0.0f});
        world.AddAngularImpulse(body, {0.0f, 100.0f, 0.0f});
    }
    world.StepFixed(10);

    EXPECT_NEAR(glm::length(world.GetLinearVelocity(floor)), 0.0f, 1e-6f);
    EXPECT_NEAR(glm::length(world.GetLinearVelocity(kinematic)), 0.0f, 1e-6f);
    EXPECT_NEAR(glm::length(world.GetAngularVelocity(kinematic)), 0.0f, 1e-6f);

    const auto kinematicPos = PositionOf(world, kinematic);
    EXPECT_NEAR(kinematicPos.x, 0.0f, 1e-5f);
    EXPECT_NEAR(kinematicPos.y, 5.0f, 1e-5f) << "kinematic bodies are not affected by gravity";
}

TEST(JoltPhysicsForces, InvalidHandleIsANoOp)
{
    fg::JoltPhysicsWorld world;
    const fg::PhysicsBodyHandle invalid{};

    world.AddImpulse(invalid, {1.0f, 0.0f, 0.0f});
    world.AddForce(invalid, {1.0f, 0.0f, 0.0f});
    world.AddTorque(invalid, {1.0f, 0.0f, 0.0f});
    world.AddAngularImpulse(invalid, {1.0f, 0.0f, 0.0f});
    world.StepFixed(1);

    EXPECT_FALSE(world.IsBodyActive(invalid));
}

// ---------------------------------------------------------------------------------------------
// Gravity and fixed step
// ---------------------------------------------------------------------------------------------

TEST(JoltPhysicsGravity, DefaultGravityIsEarthAndSetGravityRoundTrips)
{
    fg::JoltPhysicsWorld world;

    const auto defaults = world.GetGravity();
    EXPECT_NEAR(defaults.x, 0.0f, 1e-6f);
    EXPECT_NEAR(defaults.y, -9.81f, 1e-5f);
    EXPECT_NEAR(defaults.z, 0.0f, 1e-6f);

    world.SetGravity({1.0f, -3.5f, 2.0f});
    const auto custom = world.GetGravity();
    EXPECT_FLOAT_EQ(custom.x, 1.0f);
    EXPECT_FLOAT_EQ(custom.y, -3.5f);
    EXPECT_FLOAT_EQ(custom.z, 2.0f);
}

TEST(JoltPhysicsGravity, FreeFallVelocityMatchesGravityTimesElapsedTime)
{
    fg::JoltPhysicsWorld world;
    const auto body = world.CreateBody(MakeBox({0.0f, 50.0f, 0.0f}));
    ASSERT_TRUE(body.IsValid());

    constexpr int kSteps = 30;
    world.StepFixed(kSteps);

    const float expected = DampedVelocity(-9.81f, kSteps); // ~ -4.9 m/s
    const auto velocity  = world.GetLinearVelocity(body);
    EXPECT_NEAR(velocity.y, expected, std::abs(expected) * 0.01f);
    EXPECT_NEAR(velocity.x, 0.0f, 1e-5f);
    EXPECT_NEAR(velocity.z, 0.0f, 1e-5f);

    // y = y0 - g t^2 / 2 (semi-implicit Euler lands slightly lower than the analytic value).
    const float t        = kSteps * kDt;
    const float analytic = 50.0f - 0.5f * 9.81f * t * t;
    EXPECT_NEAR(PositionOf(world, body).y, analytic, 0.1f);
}

TEST(JoltPhysicsGravity, CustomGravityDirectionIsApplied)
{
    fg::JoltPhysicsWorld world;
    world.SetGravity({5.0f, 0.0f, 0.0f});
    const auto body = world.CreateBody(MakeBox({0.0f, 5.0f, 0.0f}));
    ASSERT_TRUE(body.IsValid());

    world.StepFixed(30);

    const auto velocity  = world.GetLinearVelocity(body);
    const float expected = DampedVelocity(5.0f, 30);
    EXPECT_NEAR(velocity.x, expected, expected * 0.01f);
    EXPECT_NEAR(velocity.y, 0.0f, 1e-5f);
    EXPECT_GT(PositionOf(world, body).x, 0.5f);
}

TEST(JoltPhysicsGravity, ZeroGravityKeepsRestingBodyInPlace)
{
    fg::JoltPhysicsWorld world;
    world.SetGravity({0.0f, 0.0f, 0.0f});
    const auto body = world.CreateBody(MakeBox({1.0f, 5.0f, -2.0f}));
    ASSERT_TRUE(body.IsValid());

    world.StepFixed(60);

    const auto position = PositionOf(world, body);
    EXPECT_NEAR(position.x, 1.0f, 1e-5f);
    EXPECT_NEAR(position.y, 5.0f, 1e-5f);
    EXPECT_NEAR(position.z, -2.0f, 1e-5f);
}

TEST(JoltPhysicsGravity, GetFixedDeltaTimeIsSixtyHertz)
{
    fg::JoltPhysicsWorld world;
    EXPECT_FLOAT_EQ(world.GetFixedDeltaTime(), kDt);
}

TEST(JoltPhysicsFixedStep, StepBelowFixedDeltaAccumulatesWithoutSimulating)
{
    fg::JoltPhysicsWorld world;
    const auto body = world.CreateBody(MakeBox({0.0f, 5.0f, 0.0f}));
    ASSERT_TRUE(body.IsValid());

    world.Step(0.5f * kDt);

    EXPECT_NEAR(world.GetInterpolationAlpha(), 0.5f, 1e-4f);
    EXPECT_FLOAT_EQ(world.GetLinearVelocity(body).y, 0.0f) << "no fixed tick should run yet";

    world.Step(0.5f * kDt);

    EXPECT_NEAR(world.GetInterpolationAlpha(), 0.0f, 1e-4f);
    EXPECT_LT(world.GetLinearVelocity(body).y, 0.0f) << "accumulated time completes one tick";
}

TEST(JoltPhysicsFixedStep, VariableStepRunsWholeTicksAndKeepsRemainderAsAlpha)
{
    fg::JoltPhysicsWorld variable;
    fg::JoltPhysicsWorld fixed;
    const auto a = variable.CreateBody(MakeBox({0.0f, 5.0f, 0.0f}));
    const auto b = fixed.CreateBody(MakeBox({0.0f, 5.0f, 0.0f}));
    ASSERT_TRUE(a.IsValid());
    ASSERT_TRUE(b.IsValid());

    variable.Step(2.5f * kDt);
    fixed.StepFixed(2);

    EXPECT_NEAR(variable.GetInterpolationAlpha(), 0.5f, 1e-3f);
    EXPECT_NEAR(variable.GetLinearVelocity(a).y, fixed.GetLinearVelocity(b).y, 1e-5f)
        << "Step(2.5 dt) must run exactly two fixed ticks";
    EXPECT_NEAR(PositionOf(variable, a).y, PositionOf(fixed, b).y, 1e-5f);
}

TEST(JoltPhysicsFixedStep, LargeFrameTimeIsClampedToAvoidSpiralOfDeath)
{
    fg::JoltPhysicsWorld clamped;
    fg::JoltPhysicsWorld reference;
    const auto a = clamped.CreateBody(MakeBox({0.0f, 50.0f, 0.0f}));
    const auto b = reference.CreateBody(MakeBox({0.0f, 50.0f, 0.0f}));
    ASSERT_TRUE(a.IsValid());
    ASSERT_TRUE(b.IsValid());

    clamped.Step(1.0f); // one whole second would be 60 ticks
    reference.StepFixed(5);

    const float fallSpeed    = -clamped.GetLinearVelocity(a).y;
    const float maxFallSpeed = -reference.GetLinearVelocity(b).y;
    EXPECT_GT(fallSpeed, 0.0f);
    EXPECT_LE(fallSpeed, maxFallSpeed + 1e-4f) << "accumulator is capped at 5 fixed ticks";

    const float alpha = clamped.GetInterpolationAlpha();
    EXPECT_GE(alpha, 0.0f);
    EXPECT_LT(alpha, 1.0f);
}

TEST(JoltPhysicsFixedStep, StepFixedResetsAccumulatorAndAlpha)
{
    fg::JoltPhysicsWorld world;
    const auto body = world.CreateBody(MakeBox({0.0f, 5.0f, 0.0f}));
    ASSERT_TRUE(body.IsValid());

    world.Step(0.75f * kDt);
    ASSERT_NEAR(world.GetInterpolationAlpha(), 0.75f, 1e-4f);

    world.StepFixed(1);
    EXPECT_FLOAT_EQ(world.GetInterpolationAlpha(), 0.0f);

    // The discarded 0.75 dt must not leak into the next variable step.
    world.Step(0.5f * kDt);
    EXPECT_NEAR(world.GetInterpolationAlpha(), 0.5f, 1e-4f);
}

TEST(JoltPhysicsFixedStep, SimulationIsDeterministicAcrossWorlds)
{
    fg::JoltPhysicsWorld first;
    fg::JoltPhysicsWorld second;
    for(auto *world: {&first, &second})
    {
        ASSERT_TRUE(world->CreateBody(MakeFloor()).IsValid());
    }
    const auto a = first.CreateBody(MakeBox({0.2f, 3.0f, 0.0f}));
    const auto b = second.CreateBody(MakeBox({0.2f, 3.0f, 0.0f}));
    first.AddAngularImpulse(a, {0.3f, 0.0f, 0.4f});
    second.AddAngularImpulse(b, {0.3f, 0.0f, 0.4f});

    first.StepFixed(90);
    second.StepFixed(90);

    const auto pa = PositionOf(first, a);
    const auto pb = PositionOf(second, b);
    EXPECT_NEAR(pa.x, pb.x, 1e-5f);
    EXPECT_NEAR(pa.y, pb.y, 1e-5f);
    EXPECT_NEAR(pa.z, pb.z, 1e-5f);
}

// ---------------------------------------------------------------------------------------------
// Collisions and events
// ---------------------------------------------------------------------------------------------

TEST(JoltPhysicsCollision, FallingBoxComesToRestOnFloor)
{
    fg::JoltPhysicsWorld world;
    ASSERT_TRUE(world.CreateBody(MakeFloor()).IsValid());
    const auto box = world.CreateBody(MakeBox({0.0f, 3.0f, 0.0f}));
    ASSERT_TRUE(box.IsValid());

    world.StepFixed(120);

    EXPECT_NEAR(PositionOf(world, box).y, 0.5f, 0.05f) << "box half extent above the floor top";
    EXPECT_LT(glm::length(world.GetLinearVelocity(box)), 0.1f);
}

TEST(JoltPhysicsCollision, RestingBodyFallsAsleepAndImpulseWakesIt)
{
    fg::JoltPhysicsWorld world;
    ASSERT_TRUE(world.CreateBody(MakeFloor()).IsValid());
    const auto box = world.CreateBody(MakeBox({0.0f, 0.5f, 0.0f}));
    ASSERT_TRUE(box.IsValid());
    EXPECT_TRUE(world.IsBodyActive(box));

    world.StepFixed(180);
    EXPECT_FALSE(world.IsBodyActive(box)) << "body at rest should be put to sleep";

    world.AddImpulse(box, {0.0f, 10.0f, 0.0f});
    EXPECT_TRUE(world.IsBodyActive(box)) << "impulse must wake a sleeping body";

    world.StepFixed(1);
    EXPECT_GT(PositionOf(world, box).y, 0.5f);
}

TEST(JoltPhysicsCollision, DestroyedBodyIsNotActive)
{
    fg::JoltPhysicsWorld world;
    const auto box = world.CreateBody(MakeBox({0.0f, 5.0f, 0.0f}));
    ASSERT_TRUE(world.IsBodyActive(box));

    world.DestroyBody(box);

    EXPECT_FALSE(world.IsBodyActive(box));
    glm::vec3 position{};
    glm::quat rotation{};
    EXPECT_FALSE(world.GetInterpolatedBodyPose(box, 0.5f, position, rotation));
}

TEST(JoltPhysicsCollision, BouncySphereReboundsOffFloor)
{
    fg::JoltPhysicsWorld world;
    ASSERT_TRUE(world.CreateBody(MakeFloor()).IsValid());
    auto desc        = MakeSphere({0.0f, 3.0f, 0.0f}, 0.25f);
    desc.restitution = 0.9f;
    const auto ball  = world.CreateBody(desc);
    ASSERT_TRUE(ball.IsValid());

    bool bounced = false;
    for(int i = 0; i < 120 && !bounced; ++i)
    {
        world.StepFixed(1);
        bounced = world.GetLinearVelocity(ball).y > 1.0f;
    }
    EXPECT_TRUE(bounced) << "restitution should send the sphere back up";
}

TEST(JoltPhysicsCollision, LayerMaskLetsBodyPassThroughFloor)
{
    fg::JoltPhysicsWorld world;
    ASSERT_TRUE(world.CreateBody(MakeFloor()).IsValid());

    auto ghostDesc              = MakeBox({0.0f, 1.0f, 0.0f});
    ghostDesc.collisionLayer    = kGhostLayer;
    ghostDesc.collideWithLayers = static_cast<std::uint16_t>(~(1u << kFloorLayer));
    const auto ghost            = world.CreateBody(ghostDesc);
    const auto solid            = world.CreateBody(MakeBox({3.0f, 1.0f, 0.0f}));
    ASSERT_TRUE(ghost.IsValid());
    ASSERT_TRUE(solid.IsValid());

    world.StepFixed(90);

    EXPECT_LT(PositionOf(world, ghost).y, -2.0f) << "ghost layer excludes the floor layer";
    EXPECT_NEAR(PositionOf(world, solid).y, 0.5f, 0.05f) << "default mask still hits the floor";
}

TEST(JoltPhysicsEvents, BodyContactEmitsSortedPairEventOncePerStep)
{
    constexpr std::uint64_t kFloorEntity = 7;
    constexpr std::uint64_t kBoxEntity   = 3;

    fg::JoltPhysicsWorld world;
    ASSERT_TRUE(world.CreateBody(MakeFloor(kFloorEntity)).IsValid());
    ASSERT_TRUE(world.CreateBody(MakeBox({0.0f, 1.5f, 0.0f}, 2.0f, kBoxEntity)).IsValid());

    std::vector<fg::PhysicsContactEvent> first;
    for(int i = 0; i < 90; ++i)
    {
        world.StepFixed(1);
        auto events = world.DrainContactEvents();
        // A box face touching the floor has several manifold points; the world dedupes per pair.
        EXPECT_LE(CountPair(events, kFloorEntity, kBoxEntity), 1u) << "step " << i;
        if(first.empty() && !events.empty())
        {
            first = std::move(events);
        }
    }

    ASSERT_EQ(first.size(), 1u);
    const auto &contact = first.front();
    EXPECT_EQ(contact.kind, fg::PhysicsContactKind::BodyBody);
    EXPECT_EQ(contact.entityA, kBoxEntity) << "pair is sorted: lower entity id first";
    EXPECT_EQ(contact.entityB, kFloorEntity);
    EXPECT_GT(std::abs(contact.normal.y), 0.9f) << "floor contact normal is vertical";
    EXPECT_NEAR(contact.point.y, 0.0f, 0.1f);
}

TEST(JoltPhysicsEvents, DrainContactEventsEmptiesQueue)
{
    fg::JoltPhysicsWorld world;
    ASSERT_TRUE(world.CreateBody(MakeFloor(1)).IsValid());
    // Resting on the floor top (y = 0) so the very first step reports the contact.
    ASSERT_TRUE(world.CreateBody(MakeBox({0.0f, 0.5f, 0.0f}, 2.0f, 2)).IsValid());

    world.StepFixed(1);
    EXPECT_FALSE(world.DrainContactEvents().empty());
    EXPECT_TRUE(world.DrainContactEvents().empty()) << "second drain without a step is empty";
}

TEST(JoltPhysicsEvents, BodiesWithoutEntityIdDoNotEmitContactEvents)
{
    fg::JoltPhysicsWorld world;
    ASSERT_TRUE(world.CreateBody(MakeFloor(0)).IsValid());
    const auto box = world.CreateBody(MakeBox({0.0f, 1.5f, 0.0f}, 2.0f, 0));
    ASSERT_TRUE(box.IsValid());

    world.StepFixed(90);

    EXPECT_NEAR(PositionOf(world, box).y, 0.5f, 0.05f) << "collision still happens";
    EXPECT_TRUE(world.DrainContactEvents().empty());
}

TEST(JoltPhysicsEvents, SensorReportsEnterAndExitWithoutBlockingBody)
{
    constexpr std::uint64_t kSensorEntity = 10;
    constexpr std::uint64_t kBallEntity   = 20;

    fg::JoltPhysicsWorld world;

    fg::PhysicsBodyDesc sensor{};
    sensor.motion         = fg::BodyMotionType::Static;
    sensor.shape          = fg::ColliderShape::Box;
    sensor.position       = {0.0f, 0.0f, 0.0f};
    sensor.halfExtents    = {1.0f, 1.0f, 1.0f};
    sensor.isSensor       = true;
    sensor.collisionLayer = kSensorLayer;
    sensor.entityId       = kSensorEntity;
    ASSERT_TRUE(world.CreateBody(sensor).IsValid());

    const auto ball = world.CreateBody(MakeSphere({0.0f, 3.0f, 0.0f}, 0.25f, kBallEntity));
    ASSERT_TRUE(ball.IsValid());

    std::vector<fg::TriggerEvent> triggers;
    for(int i = 0; i < 120; ++i)
    {
        world.StepFixed(1);
        auto events = world.DrainTriggerEvents();
        triggers.insert(triggers.end(), events.begin(), events.end());
    }

    ASSERT_GE(triggers.size(), 2u);
    EXPECT_EQ(triggers.front().type, fg::TriggerEventType::Enter);
    EXPECT_EQ(triggers.back().type, fg::TriggerEventType::Exit);
    for(const auto &event: triggers)
    {
        EXPECT_EQ(event.sensorEntity, kSensorEntity);
        EXPECT_EQ(event.otherEntity, kBallEntity);
    }
    const auto enters = std::count_if(triggers.begin(), triggers.end(), [](const auto &e) {
        return e.type == fg::TriggerEventType::Enter;
    });
    EXPECT_EQ(enters, 1);

    EXPECT_LT(PositionOf(world, ball).y, -2.0f) << "sensors have no collision response";
    EXPECT_TRUE(world.DrainContactEvents().empty()) << "sensor overlaps are triggers, not contacts";
}
