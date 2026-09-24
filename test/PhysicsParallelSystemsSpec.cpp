// Physics driven from parallel ECS systems: Freyr EachAsync tasks (one per archetype chunk,
// run on worker threads) calling into the Physics facade / JoltPhysicsWorld, the same way
// gameplay systems and PhysicsSystem do in Play mode.

#include "EmptyApp.hpp"

#include <Frigga/ECS/Components/NameComponent.hpp>
#include <Frigga/ECS/Components/RigidBodyComponent.hpp>
#include <Frigga/ECS/Components/TransformComponent.hpp>
#include <Frigga/ECS/Systems/PhysicsSystem.hpp>
#include <Frigga/Physics/JoltPhysicsWorld.hpp>
#include <Frigga/Physics/Physics.hpp>

#include <Freyr/Core/SystemManager.hpp>
#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>
#include <gtest/gtest.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
    constexpr float kDt = 1.0f / 60.0f;
    /// Small chunks so a few hundred entities split into many EachAsync tasks.
    constexpr std::size_t kChunkCapacity = 32;
    constexpr std::size_t kWorkerThreads = 4;

    constexpr std::uint64_t kFloorEntityId = 1;

    /// Body user data for an ECS entity (0 means "unset" in JoltPhysicsWorld).
    std::uint64_t EntityId(fr::Entity entity)
    {
        return static_cast<std::uint64_t>(entity) + 1000u;
    }

    enum class ImpulseTarget : std::uint8_t
    {
        OwnBodyViaFacade, ///< physics->AddImpulse(entity, impulses[entity])
        SharedBody,       ///< world->AddImpulse(sharedTarget, sharedImpulse)
        SharedBodyForce,  ///< world->AddForce(sharedTarget, sharedImpulse)
    };

    /// Shared state injected into the test systems. Everything read by EachAsync tasks is
    /// either immutable while systems run or written to a per-entity slot.
    struct ParallelContext
    {
        skr::Arc<fg::JoltPhysicsWorld> world;
        skr::Arc<fg::Physics> physics;

        ImpulseTarget impulseTarget = ImpulseTarget::OwnBodyViaFacade;
        bool impulsesPending        = false;
        std::unordered_map<fr::Entity, glm::vec3> impulses;
        fg::PhysicsBodyHandle sharedTarget{};
        glm::vec3 sharedImpulse{1.0f, 0.0f, 0.0f};

        int frame = 0;

        std::unordered_map<fr::Entity, std::size_t> slots;
        std::vector<fg::RaycastHit> rayHits;

        std::mutex threadMutex;
        std::set<std::thread::id> threads;

        void NoteThread()
        {
            std::lock_guard lock(threadMutex);
            threads.insert(std::this_thread::get_id());
        }
    };

    /// Gameplay-style system: applies impulses/forces from EachAsync (PreUpdate).
    class ParallelImpulseSystem: public fr::System
    {
      public:
        ParallelImpulseSystem(const skr::Arc<fr::Registry> &registry,
                              const skr::Arc<ParallelContext> &context)
            : System(registry), mContext(context)
        {
        }

        void PreUpdate(float) override
        {
            if(!mContext->impulsesPending)
            {
                return;
            }
            mContext->impulsesPending = false;

            // Capture the Arc by value: Mutation::EachAsync runs at the phase flush,
            // after this function has returned.
            mRegistry->CreateMutation()->EachAsync(
                [ctx = mContext](fr::Entity entity, fg::RigidBodyComponent &) {
                    ctx->NoteThread();
                    switch(ctx->impulseTarget)
                    {
                    case ImpulseTarget::OwnBodyViaFacade:
                        if(const auto it = ctx->impulses.find(entity); it != ctx->impulses.end())
                        {
                            ctx->physics->AddImpulse(entity, it->second);
                        }
                        break;
                    case ImpulseTarget::SharedBody:
                        ctx->world->AddImpulse(ctx->sharedTarget, ctx->sharedImpulse);
                        break;
                    case ImpulseTarget::SharedBodyForce:
                        ctx->world->AddForce(ctx->sharedTarget, ctx->sharedImpulse);
                        break;
                    }
                });
        }

      private:
        skr::Arc<ParallelContext> mContext;
    };

    /// Pushes animated kinematic poses from EachAsync (PreUpdate), like PhysicsSystem.
    class ParallelKinematicDriverSystem: public fr::System
    {
      public:
        ParallelKinematicDriverSystem(const skr::Arc<fr::Registry> &registry,
                                      const skr::Arc<ParallelContext> &context)
            : System(registry), mContext(context)
        {
        }

        static glm::vec3 PoseAt(const glm::vec3 &base, int frame)
        {
            return base + glm::vec3{0.0f, 0.1f * static_cast<float>(frame), 0.0f};
        }

        void PreUpdate(float) override
        {
            ++mContext->frame;
            mRegistry->CreateMutation()->EachAsync(
                [ctx = mContext](fr::Entity entity, fg::TransformComponent &transform,
                                 fg::RigidBodyComponent &rb) {
                    if(!rb.body.IsValid() || rb.motion != fg::BodyMotionType::Kinematic)
                    {
                        return;
                    }
                    ctx->NoteThread();
                    const auto base    = glm::vec3{static_cast<float>(entity) * 2.0f, 1.0f, 0.0f};
                    transform.position = PoseAt(base, ctx->frame);
                    ctx->world->SetTransform(rb.body, transform.position, transform.rotation);
                });
        }

      private:
        skr::Arc<ParallelContext> mContext;
    };

    /// Mirrors PhysicsSystem::Update: step on the main thread, write poses back from EachAsync.
    class ParallelStepSystem: public fr::System
    {
      public:
        ParallelStepSystem(const skr::Arc<fr::Registry> &registry,
                           const skr::Arc<ParallelContext> &context)
            : System(registry), mContext(context)
        {
        }

        void Update(float) override
        {
            mContext->world->StepFixed(1);

            mRegistry->CreateMutation()->EachAsync(
                [ctx = mContext, registry = mRegistry](fr::Entity entity,
                                                       fg::TransformComponent &transform,
                                                       fg::RigidBodyComponent &rb) {
                    if(!rb.body.IsValid() || rb.motion == fg::BodyMotionType::Kinematic)
                    {
                        return;
                    }
                    ctx->NoteThread();
                    glm::vec3 position{};
                    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
                    ctx->world->GetTransform(rb.body, position, rotation);
                    fg::WriteBodyPose(*registry, entity, transform, position, rotation);
                });
        }

      private:
        skr::Arc<ParallelContext> mContext;
    };

    /// Casts a ray straight down from every body in parallel (PostUpdate).
    class ParallelRaycastSystem: public fr::System
    {
      public:
        ParallelRaycastSystem(const skr::Arc<fr::Registry> &registry,
                              const skr::Arc<ParallelContext> &context)
            : System(registry), mContext(context)
        {
        }

        void PostUpdate(float) override
        {
            mRegistry->CreateMutation()->EachAsync(
                [ctx = mContext](fr::Entity entity, fg::TransformComponent &transform,
                                 fg::RigidBodyComponent &) {
                    const auto slot = ctx->slots.find(entity);
                    if(slot == ctx->slots.end())
                    {
                        return;
                    }
                    ctx->NoteThread();
                    fg::QueryFilter filter{};
                    filter.ignoreEntity        = EntityId(entity);
                    ctx->rayHits[slot->second] = ctx->physics->Raycast(
                        transform.position, {0.0f, -1.0f, 0.0f}, 50.0f, filter);
                });
        }

      private:
        skr::Arc<ParallelContext> mContext;
    };

    /// CharacterMovement-style gameplay (Update, after the step system): face and walk +X.
    class FacingMoverSystem: public fr::System
    {
      public:
        FacingMoverSystem(const skr::Arc<fr::Registry> &registry,
                          const skr::Arc<ParallelContext> &context)
            : System(registry), mContext(context)
        {
        }

        void Update(float) override
        {
            mRegistry->CreateMutation()->Each([ctx = mContext](fr::Entity entity,
                                                               fg::RigidBodyComponent &rb) {
                if(!rb.body.IsValid() || rb.motion != fg::BodyMotionType::Dynamic)
                {
                    return;
                }
                glm::vec3 velocity{4.0f, 0.0f, 0.0f};
                velocity.y = ctx->physics->GetCharacterVelocity(entity).y;
                ctx->physics->SetCharacterFacing(
                    entity, glm::quatLookAt(glm::vec3{-1.0f, 0.0f, 0.0f}, glm::vec3{0.0f, 1.0f, 0.0f}));
                ctx->physics->MoveCharacter(entity, velocity);
            });
        }

      private:
        skr::Arc<ParallelContext> mContext;
    };

    struct ParallelHarness
    {
        skr::Arc<skr::IApplication> app;
        skr::Arc<skr::ServiceProvider> services;
        skr::Arc<fr::Registry> registry;
        skr::Arc<fr::SystemManager> systems;
        skr::Arc<ParallelContext> context;
        std::int32_t pipeline = -1;

        static ParallelHarness Create(const glm::vec3 &gravity)
        {
            ParallelHarness harness;
            harness.app = skr::ApplicationBuilder()
                              .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension &freyr) {
                                  freyr
                                      .WithOptions([](fr::FreyrOptionsBuilder &options) {
                                          options.WithArchetypeChunkCapacity(kChunkCapacity)
                                              .WithThreadCount(kWorkerThreads);
                                      })
                                      .WithComponent<fg::NameComponent>()
                                      .WithComponent<fg::TransformComponent>()
                                      .WithComponent<fg::RigidBodyComponent>()
                                      .WithPipeline([](fr::PipelineBuilder &pipeline) {
                                          pipeline.WithName("Simulation");
                                      });
                              })
                              .Build<EmptyApp>();

            harness.services = harness.app->GetRootServiceProvider();
            harness.registry = harness.services->GetService<fr::Registry>();
            harness.systems  = harness.services->GetService<fr::SystemManager>();
            harness.pipeline = harness.systems->FindPipelineId("Simulation").value();

            harness.context        = skr::MakeArc<ParallelContext>();
            harness.context->world = skr::MakeArc<fg::JoltPhysicsWorld>();
            harness.context->physics =
                skr::MakeArc<fg::Physics>(harness.registry, harness.context->world);
            harness.context->world->SetGravity(gravity);
            harness.services->AddSingleton<ParallelContext>(harness.context);
            return harness;
        }

        template<typename TSystem>
        void AddSystem()
        {
            services->AddSingleton<TSystem>();
            systems->RegisterSystem<TSystem>(pipeline);
        }

        /// Creates one entity per desc, then physics bodies in the same order (so a serial
        /// reference world built from the same descs gets identical body ids).
        std::vector<fr::Entity> Spawn(std::vector<fg::PhysicsBodyDesc> &descs)
        {
            std::vector<fr::Entity> entities;
            entities.reserve(descs.size());
            for(std::size_t i = 0; i < descs.size(); ++i)
            {
                entities.push_back(
                    registry->CreateEntity(fg::NameComponent{.name = "Body" + std::to_string(i)},
                                           fg::TransformComponent{.position = descs[i].position},
                                           fg::RigidBodyComponent{.motion = descs[i].motion}));
            }
            registry->ExecuteTasks();

            for(std::size_t i = 0; i < descs.size(); ++i)
            {
                descs[i].entityId = EntityId(entities[i]);
                registry->TryGetComponents<fg::RigidBodyComponent>(
                    entities[i], [&](fg::RigidBodyComponent &rb) {
                        rb.body = context->world->CreateBody(descs[i]);
                    });
            }
            registry->ExecuteTasks();
            return entities;
        }

        void Tick(int frames)
        {
            for(int i = 0; i < frames; ++i)
            {
                registry->Update(kDt);
            }
        }

        [[nodiscard]] std::size_t ThreadsUsed()
        {
            std::lock_guard lock(context->threadMutex);
            return context->threads.size();
        }
    };

    fg::PhysicsBodyDesc MakeFloor()
    {
        fg::PhysicsBodyDesc floor{};
        floor.motion         = fg::BodyMotionType::Static;
        floor.shape          = fg::ColliderShape::Box;
        floor.position       = {0.0f, -0.5f, 0.0f};
        floor.halfExtents    = {200.0f, 0.5f, 200.0f};
        floor.collisionLayer = 0;
        floor.entityId       = kFloorEntityId;
        return floor;
    }

    /// @p count unit boxes on a grid with 3 m spacing (never touching each other).
    std::vector<fg::PhysicsBodyDesc> MakeBoxGrid(
        std::size_t count, float y, fg::BodyMotionType motion = fg::BodyMotionType::Dynamic)
    {
        std::vector<fg::PhysicsBodyDesc> descs;
        descs.reserve(count);
        for(std::size_t i = 0; i < count; ++i)
        {
            fg::PhysicsBodyDesc desc{};
            desc.motion         = motion;
            desc.shape          = fg::ColliderShape::Box;
            desc.position       = {static_cast<float>(i % 16) * 3.0f - 24.0f, y,
                                   static_cast<float>(i / 16) * 3.0f - 24.0f};
            desc.halfExtents    = {0.5f, 0.5f, 0.5f};
            desc.mass           = 2.0f;
            desc.collisionLayer = 1;
            descs.push_back(desc);
        }
        return descs;
    }

    glm::vec3 BodyPosition(const fg::IPhysicsWorld &world, fg::PhysicsBodyHandle body)
    {
        glm::vec3 position{};
        glm::quat rotation{};
        world.GetTransform(body, position, rotation);
        return position;
    }

    fg::PhysicsBodyHandle BodyOf(fr::Registry &registry, fr::Entity entity)
    {
        fg::PhysicsBodyHandle body{};
        registry.TryGetComponents<fg::RigidBodyComponent>(
            entity, [&](fg::RigidBodyComponent &rb) { body = rb.body; });
        return body;
    }

    glm::vec3 TransformPosition(fr::Registry &registry, fr::Entity entity)
    {
        glm::vec3 position{};
        registry.TryGetComponents<fg::TransformComponent>(
            entity, [&](fg::TransformComponent &transform) { position = transform.position; });
        return position;
    }
} // namespace

TEST(PhysicsParallelSystems, ImpulsesFromWorkerThreadsMatchSerialSimulation)
{
    constexpr std::size_t kBodies = 256;
    constexpr int kFrames         = 30;

    auto harness = ParallelHarness::Create({0.0f, 0.0f, 0.0f});
    harness.AddSystem<ParallelImpulseSystem>();
    harness.AddSystem<ParallelStepSystem>();

    auto descs    = MakeBoxGrid(kBodies, 5.0f);
    auto entities = harness.Spawn(descs);

    std::vector<glm::vec3> impulses(kBodies);
    for(std::size_t i = 0; i < kBodies; ++i)
    {
        impulses[i]                            = {0.0f, 0.5f + 0.01f * static_cast<float>(i),
                                                  static_cast<float>(i % 7) * 0.1f - 0.3f};
        harness.context->impulses[entities[i]] = impulses[i];
    }
    harness.context->impulseTarget   = ImpulseTarget::OwnBodyViaFacade;
    harness.context->impulsesPending = true;

    harness.Tick(kFrames);

    // Serial reference: same bodies in the same order, same impulses, same number of ticks.
    fg::JoltPhysicsWorld reference;
    reference.SetGravity({0.0f, 0.0f, 0.0f});
    std::vector<fg::PhysicsBodyHandle> referenceBodies;
    for(std::size_t i = 0; i < kBodies; ++i)
    {
        referenceBodies.push_back(reference.CreateBody(descs[i]));
        reference.AddImpulse(referenceBodies.back(), impulses[i]);
    }
    reference.StepFixed(kFrames);

    for(std::size_t i = 0; i < kBodies; ++i)
    {
        const auto expected = BodyPosition(reference, referenceBodies[i]);
        const auto simulated =
            BodyPosition(*harness.context->world, BodyOf(*harness.registry, entities[i]));
        const auto written = TransformPosition(*harness.registry, entities[i]);

        EXPECT_NEAR(simulated.y, expected.y, 1e-4f) << "body " << i;
        EXPECT_NEAR(simulated.z, expected.z, 1e-4f) << "body " << i;
        EXPECT_NEAR(written.x, simulated.x, 1e-4f) << "writeback " << i;
        EXPECT_NEAR(written.y, simulated.y, 1e-4f) << "writeback " << i;
        EXPECT_NEAR(written.z, simulated.z, 1e-4f) << "writeback " << i;
    }
    EXPECT_GT(harness.ThreadsUsed(), 1u) << "EachAsync work should be spread across workers";
}

TEST(PhysicsParallelSystems, ConcurrentImpulsesOnSameBodyAreNotLost)
{
    constexpr std::size_t kSources = 512;
    constexpr int kFrames          = 5;

    auto harness = ParallelHarness::Create({0.0f, 0.0f, 0.0f});
    harness.AddSystem<ParallelImpulseSystem>();

    // Sources only need a RigidBodyComponent to be iterated; they have no physics body.
    for(std::size_t i = 0; i < kSources; ++i)
    {
        harness.registry->CreateEntity(fg::NameComponent{.name = "Source"},
                                       fg::TransformComponent{}, fg::RigidBodyComponent{});
    }
    harness.registry->ExecuteTasks();

    fg::PhysicsBodyDesc target     = MakeBoxGrid(1, 5.0f).front();
    target.mass                    = 64.0f; // 1/64 is exact in binary: no rounding in the sum
    harness.context->sharedTarget  = harness.context->world->CreateBody(target);
    harness.context->impulseTarget = ImpulseTarget::SharedBody;

    for(int frame = 0; frame < kFrames; ++frame)
    {
        harness.context->impulsesPending = true;
        harness.Tick(1);
    }

    // 512 sources x 5 frames x (1 N·s / 64 kg) = 40 m/s; any lost update shows up here.
    const auto velocity = harness.context->world->GetLinearVelocity(harness.context->sharedTarget);
    EXPECT_NEAR(velocity.x, static_cast<float>(kSources * kFrames) / 64.0f, 1e-3f);
    EXPECT_NEAR(velocity.y, 0.0f, 1e-6f);
    EXPECT_GT(harness.ThreadsUsed(), 1u);
}

TEST(PhysicsParallelSystems, ConcurrentForcesOnSameBodyAccumulateIntoOneStep)
{
    constexpr std::size_t kSources = 512;

    auto harness = ParallelHarness::Create({0.0f, 0.0f, 0.0f});
    harness.AddSystem<ParallelImpulseSystem>(); // PreUpdate: forces
    harness.AddSystem<ParallelStepSystem>();    // Update: one fixed step

    for(std::size_t i = 0; i < kSources; ++i)
    {
        harness.registry->CreateEntity(fg::NameComponent{.name = "Source"},
                                       fg::TransformComponent{}, fg::RigidBodyComponent{});
    }
    harness.registry->ExecuteTasks();

    fg::PhysicsBodyDesc target       = MakeBoxGrid(1, 5.0f).front();
    target.mass                      = 64.0f;
    harness.context->sharedTarget    = harness.context->world->CreateBody(target);
    harness.context->impulseTarget   = ImpulseTarget::SharedBodyForce;
    harness.context->sharedImpulse   = {0.0f, 0.0f, 1.0f};
    harness.context->impulsesPending = true;

    harness.Tick(1);

    // a = 512 N / 64 kg = 8 m/s^2 for one tick, then Jolt linear damping (0.05).
    const float expected = 8.0f * kDt * (1.0f - 0.05f * kDt);
    const auto velocity  = harness.context->world->GetLinearVelocity(harness.context->sharedTarget);
    EXPECT_NEAR(velocity.z, expected, expected * 0.005f);
}

TEST(PhysicsParallelSystems, KinematicPosesPushedFromWorkersReachPhysicsWorld)
{
    constexpr std::size_t kBodies = 256;
    constexpr int kFrames         = 10;

    auto harness = ParallelHarness::Create({0.0f, -9.81f, 0.0f});
    harness.AddSystem<ParallelKinematicDriverSystem>();
    harness.AddSystem<ParallelStepSystem>();

    auto descs    = MakeBoxGrid(kBodies, 1.0f, fg::BodyMotionType::Kinematic);
    auto entities = harness.Spawn(descs);

    harness.Tick(kFrames);

    for(const auto entity: entities)
    {
        const auto base     = glm::vec3{static_cast<float>(entity) * 2.0f, 1.0f, 0.0f};
        const auto expected = ParallelKinematicDriverSystem::PoseAt(base, kFrames);
        const auto actual =
            BodyPosition(*harness.context->world, BodyOf(*harness.registry, entity));
        EXPECT_NEAR(actual.x, expected.x, 1e-4f) << "entity " << entity;
        EXPECT_NEAR(actual.y, expected.y, 1e-4f) << "kinematic bodies ignore gravity";
        EXPECT_NEAR(actual.z, expected.z, 1e-4f);
    }
    EXPECT_GT(harness.ThreadsUsed(), 1u);
}

TEST(PhysicsParallelSystems, ParallelRaycastsHitFloorFromEveryBody)
{
    constexpr std::size_t kBodies = 256;

    auto harness = ParallelHarness::Create({0.0f, 0.0f, 0.0f}); // bodies hover in place
    harness.AddSystem<ParallelRaycastSystem>();

    ASSERT_TRUE(harness.context->world->CreateBody(MakeFloor()).IsValid());

    auto descs = MakeBoxGrid(kBodies, 0.0f);
    for(std::size_t i = 0; i < kBodies; ++i)
    {
        descs[i].position.y = 2.0f + static_cast<float>(i % 5); // 2..6 m above the floor
    }
    auto entities = harness.Spawn(descs);

    harness.context->rayHits.assign(kBodies, fg::RaycastHit{});
    for(std::size_t i = 0; i < kBodies; ++i)
    {
        harness.context->slots[entities[i]] = i;
    }

    harness.Tick(3);

    for(std::size_t i = 0; i < kBodies; ++i)
    {
        const auto &hit = harness.context->rayHits[i];
        ASSERT_TRUE(hit.hit) << "body " << i;
        EXPECT_EQ(hit.entityId, kFloorEntityId) << "ray must skip the caster and hit the floor";
        EXPECT_NEAR(hit.distance, descs[i].position.y, 1e-3f) << "body " << i;
        EXPECT_GT(hit.normal.y, 0.9f);
    }
    EXPECT_GT(harness.ThreadsUsed(), 1u);
}

TEST(PhysicsParallelSystems, CharacterFacingDoesNotUndoTheStepBeforeWriteBack)
{
    constexpr int kFrames = 60;

    auto harness = ParallelHarness::Create({0.0f, -9.81f, 0.0f});
    harness.AddSystem<ParallelStepSystem>(); // write-back is deferred to the phase flush
    harness.AddSystem<FacingMoverSystem>();  // so the Transform still holds last tick's pose

    ASSERT_TRUE(harness.context->world->CreateBody(MakeFloor()).IsValid());
    std::vector<fg::PhysicsBodyDesc> descs(1);
    descs[0].motion         = fg::BodyMotionType::Dynamic;
    descs[0].shape          = fg::ColliderShape::Sphere;
    descs[0].radius         = 0.5f;
    descs[0].position       = {0.0f, 0.5f, 0.0f};
    descs[0].mass           = 70.0f;
    descs[0].collisionLayer = 1;
    descs[0].lockRotationX = descs[0].lockRotationY = descs[0].lockRotationZ = true;
    const auto player = harness.Spawn(descs).front();

    harness.Tick(kFrames);

    // 4 m/s for 1 s (minus friction/damping); a facing that re-teleports to the stale
    // Transform pins the body at x = 0.
    const auto body = BodyPosition(*harness.context->world, BodyOf(*harness.registry, player));
    EXPECT_GT(body.x, 3.0f);
    EXPECT_NEAR(TransformPosition(*harness.registry, player).x, body.x, 0.1f);
}

TEST(PhysicsParallelSystems, ContactEventsFromManySimultaneousLandingsAreCompleteAndDeduped)
{
    constexpr std::size_t kBodies = 256;
    constexpr int kFrames         = 90;

    auto harness = ParallelHarness::Create({0.0f, -9.81f, 0.0f});
    harness.AddSystem<ParallelStepSystem>();

    ASSERT_TRUE(harness.context->world->CreateBody(MakeFloor()).IsValid());
    auto descs    = MakeBoxGrid(kBodies, 1.5f);
    auto entities = harness.Spawn(descs);

    std::unordered_set<std::uint64_t> landed;
    for(int frame = 0; frame < kFrames; ++frame)
    {
        harness.Tick(1);

        // Jolt reports contacts from its own job threads; the world must dedupe per pair.
        std::set<std::pair<std::uint64_t, std::uint64_t>> pairsThisStep;
        for(const auto &event: harness.context->world->DrainContactEvents())
        {
            EXPECT_TRUE(pairsThisStep.emplace(event.entityA, event.entityB).second)
                << "duplicate pair (" << event.entityA << ", " << event.entityB << ") in frame "
                << frame;
            if(event.entityA == kFloorEntityId)
            {
                landed.insert(event.entityB);
            }
        }
    }

    EXPECT_EQ(landed.size(), kBodies) << "every box should report touching the floor";
    for(const auto entity: entities)
    {
        EXPECT_TRUE(landed.contains(EntityId(entity))) << "entity " << entity;
        EXPECT_NEAR(TransformPosition(*harness.registry, entity).y, 0.5f, 0.05f)
            << "box resting on the floor, written back by the parallel system";
    }
}
