#include <Frigga/Physics/JoltPhysicsWorld.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <glm/gtc/quaternion.hpp>

JPH_SUPPRESS_WARNINGS

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        constexpr unsigned kMaxBodies             = 65536;
        constexpr unsigned kMaxBodyPairs          = 65536;
        constexpr unsigned kMaxContactConstraints = 20480;
        constexpr unsigned kLayerCount            = 16;
        constexpr float    kFixedDeltaTime        = 1.0f / 60.0f;

        void TraceImpl(const char *fmt, ...)
        {
            char buffer[1024];
            va_list args;
            va_start(args, fmt);
            std::vsnprintf(buffer, sizeof(buffer), fmt, args);
            va_end(args);
            std::cerr << "[Jolt] " << buffer << '\n';
        }

#ifdef JPH_ENABLE_ASSERTS
        bool AssertFailedImpl(const char *expression, const char *message, const char *file,
                              unsigned line)
        {
            std::cerr << "[Jolt Assert] " << file << ':' << line << " (" << expression << ") "
                      << (message != nullptr ? message : "") << '\n';
            return true;
        }
#endif

        namespace BroadPhaseLayers
        {
            constexpr JPH::BroadPhaseLayer NonMoving {0};
            constexpr JPH::BroadPhaseLayer Moving {1};
            constexpr unsigned             NumLayers = 2;
        } // namespace BroadPhaseLayers

        class BPLayerInterfaceImpl final: public JPH::BroadPhaseLayerInterface
        {
          public:
            explicit BPLayerInterfaceImpl(const std::array<bool, kLayerCount> &layerIsMoving)
                : mLayerIsMoving(layerIsMoving)
            {
            }

            [[nodiscard]] unsigned GetNumBroadPhaseLayers() const override
            {
                return BroadPhaseLayers::NumLayers;
            }

            [[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(
                JPH::ObjectLayer layer) const override
            {
                const bool moving = layer < kLayerCount && mLayerIsMoving[layer];
                return moving ? BroadPhaseLayers::Moving : BroadPhaseLayers::NonMoving;
            }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
            [[nodiscard]] const char *GetBroadPhaseLayerName(
                JPH::BroadPhaseLayer layer) const override
            {
                switch((JPH::BroadPhaseLayer::Type)layer)
                {
                case(JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NonMoving:
                    return "NON_MOVING";
                case(JPH::BroadPhaseLayer::Type)BroadPhaseLayers::Moving:
                    return "MOVING";
                default:
                    return "INVALID";
                }
            }
#endif

          private:
            const std::array<bool, kLayerCount> &mLayerIsMoving;
        };

        class ObjectVsBroadPhaseLayerFilterImpl final: public JPH::ObjectVsBroadPhaseLayerFilter
        {
          public:
            explicit ObjectVsBroadPhaseLayerFilterImpl(
                const std::array<bool, kLayerCount> &layerIsMoving)
                : mLayerIsMoving(layerIsMoving)
            {
            }

            [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer layer,
                                             JPH::BroadPhaseLayer broadPhase) const override
            {
                const bool moving = layer < kLayerCount ? mLayerIsMoving[layer] : true;
                if(moving)
                {
                    return true;
                }
                return broadPhase == BroadPhaseLayers::Moving;
            }

          private:
            const std::array<bool, kLayerCount> &mLayerIsMoving;
        };

        class ObjectLayerPairFilterImpl final: public JPH::ObjectLayerPairFilter
        {
          public:
            explicit ObjectLayerPairFilterImpl(const std::array<std::uint16_t, kLayerCount> &masks)
                : mMasks(masks)
            {
            }

            [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override
            {
                if(a >= kLayerCount || b >= kLayerCount)
                {
                    return false;
                }
                return (mMasks[a] & (1u << b)) != 0 && (mMasks[b] & (1u << a)) != 0;
            }

          private:
            const std::array<std::uint16_t, kLayerCount> &mMasks;
        };

        class MaskObjectLayerFilter final: public JPH::ObjectLayerFilter
        {
          public:
            explicit MaskObjectLayerFilter(std::uint16_t mask): mMask(mask)
            {
            }

            [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer layer) const override
            {
                if(layer >= kLayerCount)
                {
                    return false;
                }
                return (mMask & (1u << layer)) != 0;
            }

          private:
            std::uint16_t mMask;
        };

        class AcceptAllBroadPhaseLayerFilter final: public JPH::BroadPhaseLayerFilter
        {
          public:
            [[nodiscard]] bool ShouldCollide(JPH::BroadPhaseLayer) const override
            {
                return true;
            }
        };

        class IgnoreEntityBodyFilter final: public JPH::BodyFilter
        {
          public:
            explicit IgnoreEntityBodyFilter(std::uint64_t ignoreEntity)
                : mIgnoreEntity(ignoreEntity)
            {
            }

            [[nodiscard]] bool ShouldCollideLocked(const JPH::Body &body) const override
            {
                if(mIgnoreEntity == 0)
                {
                    return true;
                }
                return body.GetUserData() != mIgnoreEntity;
            }

          private:
            std::uint64_t mIgnoreEntity;
        };

        JPH::EMotionType ToJoltMotion(BodyMotionType motion)
        {
            switch(motion)
            {
            case BodyMotionType::Static:
                return JPH::EMotionType::Static;
            case BodyMotionType::Kinematic:
                return JPH::EMotionType::Kinematic;
            case BodyMotionType::Dynamic:
                return JPH::EMotionType::Dynamic;
            }
            return JPH::EMotionType::Dynamic;
        }

        [[nodiscard]] std::pair<std::uint64_t, std::uint64_t> SortedEntityPair(std::uint64_t a,
                                                                               std::uint64_t b)
        {
            return a <= b ? std::pair {a, b} : std::pair {b, a};
        }

        struct EntityPairHash
        {
            std::size_t operator()(const std::pair<std::uint64_t, std::uint64_t> &p) const noexcept
            {
                const auto h1 = std::hash<std::uint64_t> {}(p.first);
                const auto h2 = std::hash<std::uint64_t> {}(p.second);
                return h1 ^ (h2 + 0x9e3779b97f4a7c15ull + (h1 << 6) + (h1 >> 2));
            }
        };

        [[nodiscard]] std::uint64_t PackBodyPair(JPH::BodyID a, JPH::BodyID b)
        {
            std::uint64_t lo = a.GetIndexAndSequenceNumber();
            std::uint64_t hi = b.GetIndexAndSequenceNumber();
            if(lo > hi)
            {
                std::swap(lo, hi);
            }
            return (hi << 32) | lo;
        }

        std::once_flag gJoltInitOnce;

        void EnsureJoltInitialized()
        {
            std::call_once(gJoltInitOnce, [] {
                JPH::RegisterDefaultAllocator();
                JPH::Trace = TraceImpl;
                JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertFailedImpl;)
                JPH::Factory::sInstance = new JPH::Factory();
                JPH::RegisterTypes();
            });
        }

        struct CharacterEntry
        {
            JPH::Ref<JPH::CharacterVirtual> character;
            JPH::ObjectLayer                layer                 = 0;
            float                           stickToFloorDistance  = 0.5f;
            float                           walkStairsStepHeight  = 0.4f;
            std::uint64_t                   entityId              = 0;
            glm::vec3                       prevPosition {0.0f};
            glm::vec3                       currPosition {0.0f};
        };

        struct BodyPoseSample
        {
            glm::vec3 prevPosition {0.0f};
            glm::vec3 currPosition {0.0f};
            glm::quat prevRotation {1.0f, 0.0f, 0.0f, 0.0f};
            glm::quat currRotation {1.0f, 0.0f, 0.0f, 0.0f};
        };

        struct ActiveTriggerContact
        {
            std::uint64_t sensorEntity = 0;
            std::uint64_t otherEntity  = 0;
        };

        [[nodiscard]] JPH::RefConst<JPH::Shape> MakeStandingCapsule(
            float radius, float height, const glm::vec3 &centerOffset)
        {
            using namespace JPH;
            const float r          = std::max(radius, 0.001f);
            const float halfHeight = std::max(0.5f * height, 0.001f);
            RefConst<Shape> capsule = new CapsuleShape(halfHeight, r);
            return new RotatedTranslatedShape(
                Vec3(centerOffset.x, halfHeight + r + centerOffset.y, centerOffset.z),
                Quat::sIdentity(), capsule);
        }

        template <typename VecT>
        [[nodiscard]] glm::vec3 ToGlmVec(const VecT &v)
        {
            return {static_cast<float>(v.GetX()), static_cast<float>(v.GetY()),
                    static_cast<float>(v.GetZ())};
        }

        [[nodiscard]] glm::quat ToGlmQuat(JPH::QuatArg q)
        {
            return {q.GetW(), q.GetX(), q.GetY(), q.GetZ()};
        }
    } // namespace

    struct JoltPhysicsWorld::Impl
    {
        struct WorldContactListener final: public JPH::ContactListener
        {
            Impl *owner = nullptr;

            void OnContactAdded(const JPH::Body &body1, const JPH::Body &body2,
                                const JPH::ContactManifold &manifold,
                                JPH::ContactSettings &) override
            {
                if(owner != nullptr)
                {
                    owner->HandleBodyContact(body1, body2, manifold, TriggerEventType::Enter, true);
                }
            }

            void OnContactPersisted(const JPH::Body &body1, const JPH::Body &body2,
                                    const JPH::ContactManifold &manifold,
                                    JPH::ContactSettings &) override
            {
                if(owner != nullptr)
                {
                    owner->HandleBodyContact(body1, body2, manifold, TriggerEventType::Stay, false);
                }
            }

            void OnContactRemoved(const JPH::SubShapeIDPair &pair) override
            {
                if(owner != nullptr)
                {
                    owner->HandleBodyContactRemoved(pair);
                }
            }
        };

        struct WorldCharacterContactListener final: public JPH::CharacterContactListener
        {
            Impl *owner = nullptr;

            void OnContactAdded(const JPH::CharacterVirtual *character, const JPH::BodyID &bodyId,
                                const JPH::SubShapeID &, JPH::RVec3Arg contactPosition,
                                JPH::Vec3Arg contactNormal,
                                JPH::CharacterContactSettings &) override
            {
                if(owner != nullptr)
                {
                    owner->HandleCharacterContact(character, bodyId, contactPosition, contactNormal,
                                                  0.0f);
                }
            }

            void OnContactPersisted(const JPH::CharacterVirtual *character,
                                    const JPH::BodyID &bodyId, const JPH::SubShapeID &,
                                    JPH::RVec3Arg contactPosition, JPH::Vec3Arg contactNormal,
                                    JPH::CharacterContactSettings &) override
            {
                if(owner != nullptr)
                {
                    owner->HandleCharacterContact(character, bodyId, contactPosition, contactNormal,
                                                  0.0f);
                }
            }
        };

        Impl()
            : tempAllocator(64 * 1024 * 1024),
              jobSystem(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
                        static_cast<int>(std::max(1u, std::thread::hardware_concurrency()) - 1)),
              broadPhase(layerIsMoving), objectVsBroadphase(layerIsMoving),
              objectVsObject(layerMasks)
        {
            layerMasks.fill(0);
            layerIsMoving.fill(false);
            layerBodyCount.fill(0);

            physicsSystem.Init(kMaxBodies, 0, kMaxBodyPairs, kMaxContactConstraints, broadPhase,
                               objectVsBroadphase, objectVsObject);
            physicsSystem.SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));

            contactListener.owner          = this;
            characterContactListener.owner = this;
            physicsSystem.SetContactListener(&contactListener);
        }

        ~Impl()
        {
            physicsSystem.SetContactListener(nullptr);
        }

        void NoteLayer(std::uint8_t collisionLayer, std::uint16_t collideWithLayers, bool moving)
        {
            const auto layer = static_cast<JPH::ObjectLayer>(
                std::min<std::uint8_t>(collisionLayer, kLayerCount - 1));
            if(layerBodyCount[layer] == 0)
            {
                layerMasks[layer] = collideWithLayers;
            }
            else
            {
                layerMasks[layer] |= collideWithLayers;
            }
            ++layerBodyCount[layer];
            layerIsMoving[layer] = layerIsMoving[layer] || moving;
        }

        void HandleBodyContact(const JPH::Body &body1, const JPH::Body &body2,
                               const JPH::ContactManifold &manifold, TriggerEventType triggerType,
                               bool isAdded)
        {
            const auto entity1 = static_cast<std::uint64_t>(body1.GetUserData());
            const auto entity2 = static_cast<std::uint64_t>(body2.GetUserData());
            if(entity1 == 0 || entity2 == 0)
            {
                return;
            }

            const glm::vec3 point =
                manifold.mRelativeContactPointsOn1.empty()
                    ? ToGlmVec(manifold.mBaseOffset)
                    : ToGlmVec(manifold.GetWorldSpaceContactPointOn1(0));
            const glm::vec3 normal = ToGlmVec(manifold.mWorldSpaceNormal);
            const float     pen    = manifold.mPenetrationDepth;

            std::lock_guard lock(eventMutex);

            if(body1.IsSensor() || body2.IsSensor())
            {
                const std::uint64_t sensorEntity = body1.IsSensor() ? entity1 : entity2;
                const std::uint64_t otherEntity  = body1.IsSensor() ? entity2 : entity1;
                const auto          key = PackBodyPair(body1.GetID(), body2.GetID());

                if(isAdded)
                {
                    activeTriggers[key] = ActiveTriggerContact {sensorEntity, otherEntity};
                }

                pendingTriggers.push_back(TriggerEvent {
                    .type         = triggerType,
                    .sensorEntity = sensorEntity,
                    .otherEntity  = otherEntity,
                });
                return;
            }

            const auto pairKey = SortedEntityPair(entity1, entity2);
            if(!contactDedupe.insert(pairKey).second)
            {
                return;
            }

            pendingContacts.push_back(PhysicsContactEvent {
                .kind        = PhysicsContactKind::BodyBody,
                .entityA     = pairKey.first,
                .entityB     = pairKey.second,
                .point       = point,
                .normal      = normal,
                .penetration = pen,
            });
        }

        void HandleBodyContactRemoved(const JPH::SubShapeIDPair &pair)
        {
            const auto key = PackBodyPair(pair.GetBody1ID(), pair.GetBody2ID());

            std::lock_guard lock(eventMutex);
            const auto      it = activeTriggers.find(key);
            if(it == activeTriggers.end())
            {
                return;
            }

            pendingTriggers.push_back(TriggerEvent {
                .type         = TriggerEventType::Exit,
                .sensorEntity = it->second.sensorEntity,
                .otherEntity  = it->second.otherEntity,
            });
            activeTriggers.erase(it);
        }

        void HandleCharacterContact(const JPH::CharacterVirtual *character,
                                    const JPH::BodyID &bodyId, JPH::RVec3Arg contactPosition,
                                    JPH::Vec3Arg contactNormal, float penetration)
        {
            if(character == nullptr)
            {
                return;
            }

            const auto characterEntity = static_cast<std::uint64_t>(character->GetUserData());
            if(characterEntity == 0)
            {
                return;
            }

            auto &bodyInterface = physicsSystem.GetBodyInterface();
            if(!bodyInterface.IsAdded(bodyId))
            {
                return;
            }

            const auto bodyEntity = static_cast<std::uint64_t>(bodyInterface.GetUserData(bodyId));
            if(bodyEntity == 0)
            {
                return;
            }

            const auto pairKey = SortedEntityPair(characterEntity, bodyEntity);

            std::lock_guard lock(eventMutex);
            if(!contactDedupe.insert(pairKey).second)
            {
                return;
            }

            pendingContacts.push_back(PhysicsContactEvent {
                .kind        = PhysicsContactKind::CharacterBody,
                .entityA     = characterEntity,
                .entityB     = bodyEntity,
                .point       = ToGlmVec(contactPosition),
                .normal      = ToGlmVec(contactNormal),
                .penetration = penetration,
            });
        }

        JPH::TempAllocatorImpl tempAllocator;
        JPH::JobSystemThreadPool jobSystem;
        std::array<bool, kLayerCount> layerIsMoving {};
        std::array<std::uint16_t, kLayerCount> layerMasks {};
        std::array<std::uint16_t, kLayerCount> layerBodyCount {};
        BPLayerInterfaceImpl broadPhase;
        ObjectVsBroadPhaseLayerFilterImpl objectVsBroadphase;
        ObjectLayerPairFilterImpl objectVsObject;
        JPH::PhysicsSystem physicsSystem;
        WorldContactListener contactListener;
        WorldCharacterContactListener characterContactListener;

        float accumulator         = 0.0f;
        float interpolationAlpha  = 0.0f;

        std::unordered_map<std::uint32_t, CharacterEntry> characters;
        std::unordered_map<std::uint64_t, PhysicsCharacterHandle> entityCharacters;
        std::unordered_map<std::uint32_t, BodyPoseSample> bodyPoses;
        std::unordered_map<std::uint32_t, JPH::Ref<JPH::Constraint>> joints;

        std::mutex eventMutex;
        std::unordered_set<std::pair<std::uint64_t, std::uint64_t>, EntityPairHash> contactDedupe;
        std::unordered_map<std::uint64_t, ActiveTriggerContact> activeTriggers;
        std::vector<TriggerEvent> pendingTriggers;
        std::vector<TriggerEvent> readyTriggers;
        std::vector<PhysicsContactEvent> pendingContacts;
        std::vector<PhysicsContactEvent> readyContacts;

        std::uint32_t nextCharacterId = 1;
        std::uint32_t nextJointId     = 1;
    };

    JoltPhysicsWorld::JoltPhysicsWorld()
    {
        EnsureJoltInitialized();
        mImpl = std::make_unique<Impl>();
    }

    JoltPhysicsWorld::~JoltPhysicsWorld()
    {
        Clear();
        mImpl.reset();
    }

    void JoltPhysicsWorld::Clear()
    {
        for(auto &[id, constraint]: mImpl->joints)
        {
            if(constraint != nullptr)
            {
                mImpl->physicsSystem.RemoveConstraint(constraint);
            }
        }
        mImpl->joints.clear();

        for(auto &[id, entry]: mImpl->characters)
        {
            if(entry.character != nullptr)
            {
                entry.character->SetListener(nullptr);
            }
        }
        mImpl->characters.clear();
        mImpl->entityCharacters.clear();

        {
            std::lock_guard lock(mImpl->eventMutex);
            mImpl->pendingTriggers.clear();
            mImpl->readyTriggers.clear();
            mImpl->pendingContacts.clear();
            mImpl->readyContacts.clear();
            mImpl->contactDedupe.clear();
            mImpl->activeTriggers.clear();
        }

        mImpl->bodyPoses.clear();

        auto &bodyInterface = mImpl->physicsSystem.GetBodyInterface();
        JPH::BodyIDVector bodies;
        mImpl->physicsSystem.GetBodies(bodies);
        for(const JPH::BodyID id: bodies)
        {
            bodyInterface.RemoveBody(id);
            bodyInterface.DestroyBody(id);
        }

        mImpl->layerMasks.fill(0);
        mImpl->layerIsMoving.fill(false);
        mImpl->layerBodyCount.fill(0);
        mImpl->accumulator        = 0.0f;
        mImpl->interpolationAlpha = 0.0f;
        mImpl->nextCharacterId    = 1;
        mImpl->nextJointId        = 1;
    }

    void JoltPhysicsWorld::OptimizeBroadPhase()
    {
        mImpl->physicsSystem.OptimizeBroadPhase();
    }

    float JoltPhysicsWorld::GetFixedDeltaTime() const
    {
        return kFixedDeltaTime;
    }

    float JoltPhysicsWorld::GetInterpolationAlpha() const
    {
        return mImpl->interpolationAlpha;
    }

    void JoltPhysicsWorld::updateCharactersFixed()
    {
        using namespace JPH;

        const Vec3 worldGravity = mImpl->physicsSystem.GetGravity();

        for(auto &[id, entry]: mImpl->characters)
        {
            if(entry.character == nullptr)
            {
                continue;
            }

            // CharacterVirtual does not integrate freefall gravity; callers must.
            // inGravity on ExtendedUpdate only pushes down onto supporting bodies.
            Vec3 velocity = entry.character->GetLinearVelocity();
            velocity += worldGravity * kFixedDeltaTime;
            entry.character->SetLinearVelocity(velocity);

            CharacterVirtual::ExtendedUpdateSettings updateSettings;
            if(entry.stickToFloorDistance > 0.0f)
            {
                updateSettings.mStickToFloorStepDown =
                    Vec3(0.0f, -entry.stickToFloorDistance, 0.0f);
            }
            else
            {
                updateSettings.mStickToFloorStepDown = Vec3::sZero();
            }
            if(entry.walkStairsStepHeight > 0.0f)
            {
                updateSettings.mWalkStairsStepUp = Vec3(0.0f, entry.walkStairsStepHeight, 0.0f);
            }
            else
            {
                updateSettings.mWalkStairsStepUp = Vec3::sZero();
            }
            entry.character->ExtendedUpdate(
                kFixedDeltaTime, worldGravity, updateSettings,
                mImpl->physicsSystem.GetDefaultBroadPhaseLayerFilter(entry.layer),
                mImpl->physicsSystem.GetDefaultLayerFilter(entry.layer), {}, {},
                mImpl->tempAllocator);
        }
    }

    void JoltPhysicsWorld::updateBodyInterpolationSamples()
    {
        auto &bodyInterface = mImpl->physicsSystem.GetBodyInterface();
        JPH::BodyIDVector bodies;
        mImpl->physicsSystem.GetBodies(bodies);

        for(const JPH::BodyID id: bodies)
        {
            if(!bodyInterface.IsAdded(id))
            {
                continue;
            }

            JPH::RVec3 pos;
            JPH::Quat  rot;
            bodyInterface.GetPositionAndRotation(id, pos, rot);

            const auto key = id.GetIndexAndSequenceNumber();
            auto       it  = mImpl->bodyPoses.find(key);
            if(it == mImpl->bodyPoses.end())
            {
                BodyPoseSample sample;
                sample.prevPosition = ToGlmVec(pos);
                sample.currPosition = sample.prevPosition;
                sample.prevRotation = ToGlmQuat(rot);
                sample.currRotation = sample.prevRotation;
                mImpl->bodyPoses.emplace(key, sample);
                continue;
            }

            it->second.prevPosition = it->second.currPosition;
            it->second.prevRotation = it->second.currRotation;
            it->second.currPosition = ToGlmVec(pos);
            it->second.currRotation = ToGlmQuat(rot);
        }
    }

    void JoltPhysicsWorld::updateCharacterInterpolationSamples()
    {
        for(auto &[id, entry]: mImpl->characters)
        {
            if(entry.character == nullptr)
            {
                continue;
            }
            entry.prevPosition = entry.currPosition;
            entry.currPosition = ToGlmVec(entry.character->GetPosition());
        }
    }

    void JoltPhysicsWorld::flushPendingEvents()
    {
        std::lock_guard lock(mImpl->eventMutex);
        mImpl->readyTriggers.insert(mImpl->readyTriggers.end(), mImpl->pendingTriggers.begin(),
                                    mImpl->pendingTriggers.end());
        mImpl->pendingTriggers.clear();
        mImpl->readyContacts.insert(mImpl->readyContacts.end(), mImpl->pendingContacts.begin(),
                                    mImpl->pendingContacts.end());
        mImpl->pendingContacts.clear();
    }

    void JoltPhysicsWorld::stepFixedInternal(int steps)
    {
        const int count = std::max(steps, 0);
        for(int i = 0; i < count; ++i)
        {
            {
                std::lock_guard lock(mImpl->eventMutex);
                mImpl->contactDedupe.clear();
            }

            mImpl->physicsSystem.Update(kFixedDeltaTime, 1, &mImpl->tempAllocator,
                                        &mImpl->jobSystem);
            updateCharactersFixed();
            updateBodyInterpolationSamples();
            updateCharacterInterpolationSamples();
            flushPendingEvents();
        }
    }

    void JoltPhysicsWorld::Step(float deltaTime)
    {
        mImpl->accumulator += deltaTime;
        mImpl->accumulator = std::min(mImpl->accumulator, kFixedDeltaTime * 5.0f);

        while(mImpl->accumulator >= kFixedDeltaTime)
        {
            stepFixedInternal(1);
            mImpl->accumulator -= kFixedDeltaTime;
        }

        mImpl->interpolationAlpha = mImpl->accumulator / kFixedDeltaTime;
    }

    void JoltPhysicsWorld::StepFixed(int steps)
    {
        mImpl->accumulator        = 0.0f;
        mImpl->interpolationAlpha = 0.0f;
        stepFixedInternal(steps);
    }

    PhysicsBodyHandle JoltPhysicsWorld::CreateBody(const PhysicsBodyDesc &desc)
    {
        using namespace JPH;

        const auto layer =
            static_cast<ObjectLayer>(std::min<std::uint8_t>(desc.collisionLayer, kLayerCount - 1));
        mImpl->NoteLayer(desc.collisionLayer, desc.collideWithLayers,
                         desc.motion == BodyMotionType::Dynamic ||
                             desc.motion == BodyMotionType::Kinematic);

        RefConst<Shape> shape;
        switch(desc.shape)
        {
        case ColliderShape::Box: {
            const Vec3 half {std::max(desc.halfExtents.x * desc.scale.x, 0.001f),
                             std::max(desc.halfExtents.y * desc.scale.y, 0.001f),
                             std::max(desc.halfExtents.z * desc.scale.z, 0.001f)};
            shape = new BoxShape(half);
            break;
        }
        case ColliderShape::Sphere: {
            const float radius = std::max(
                desc.radius * std::max({desc.scale.x, desc.scale.y, desc.scale.z}), 0.001f);
            shape = new SphereShape(radius);
            break;
        }
        case ColliderShape::Capsule: {
            const float radius =
                std::max(desc.radius * std::max(desc.scale.x, desc.scale.z), 0.001f);
            const float halfHeight = std::max(0.5f * desc.height * desc.scale.y, 0.001f);
            shape                  = new CapsuleShape(halfHeight, radius);
            break;
        }
        case ColliderShape::Mesh: {
            Array<Vec3> points;
            points.reserve(desc.meshPoints.size());
            for(const auto &p: desc.meshPoints)
            {
                points.push_back(Vec3(p.x * desc.scale.x, p.y * desc.scale.y, p.z * desc.scale.z));
            }
            if(points.size() < 3)
            {
                shape = new BoxShape(Vec3(0.5f, 0.5f, 0.5f));
            }
            else
            {
                ConvexHullShapeSettings settings(points);
                settings.SetEmbedded();
                const ShapeSettings::ShapeResult result = settings.Create();
                if(result.HasError())
                {
                    shape = new BoxShape(Vec3(0.5f, 0.5f, 0.5f));
                }
                else
                {
                    shape = result.Get();
                }
            }
            break;
        }
        }

        const RVec3 position(desc.position.x, desc.position.y, desc.position.z);
        const Quat rotation(desc.rotation.x, desc.rotation.y, desc.rotation.z, desc.rotation.w);

        BodyCreationSettings settings(shape, position, rotation, ToJoltMotion(desc.motion), layer);
        settings.mFriction    = desc.friction;
        settings.mRestitution = desc.restitution;
        settings.mIsSensor    = desc.isSensor;
        settings.mUserData    = desc.entityId;
        if(desc.motion == BodyMotionType::Dynamic)
        {
            settings.mOverrideMassProperties       = EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = std::max(desc.mass, 0.001f);
        }

        BodyInterface &bodyInterface = mImpl->physicsSystem.GetBodyInterface();
        const BodyID id              = bodyInterface.CreateAndAddBody(
            settings, desc.motion == BodyMotionType::Static ? EActivation::DontActivate
                                                            : EActivation::Activate);
        if(id.IsInvalid())
        {
            return {};
        }

        const auto handleId = id.GetIndexAndSequenceNumber();
        BodyPoseSample sample;
        sample.prevPosition = desc.position;
        sample.currPosition = desc.position;
        sample.prevRotation = desc.rotation;
        sample.currRotation = desc.rotation;
        mImpl->bodyPoses[handleId] = sample;

        return PhysicsBodyHandle {.id = handleId};
    }

    void JoltPhysicsWorld::DestroyBody(PhysicsBodyHandle handle)
    {
        if(!handle.IsValid())
        {
            return;
        }

        mImpl->bodyPoses.erase(handle.id);

        const JPH::BodyID id(handle.id);
        auto &bodyInterface = mImpl->physicsSystem.GetBodyInterface();
        if(bodyInterface.IsAdded(id))
        {
            bodyInterface.RemoveBody(id);
        }
        bodyInterface.DestroyBody(id);
    }

    void JoltPhysicsWorld::SetTransform(PhysicsBodyHandle handle, const glm::vec3 &position,
                                        const glm::quat &rotation)
    {
        if(!handle.IsValid())
        {
            return;
        }

        const JPH::BodyID id(handle.id);
        auto &bodyInterface = mImpl->physicsSystem.GetBodyInterface();
        bodyInterface.SetPositionAndRotation(
            id, JPH::RVec3(position.x, position.y, position.z),
            JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w), JPH::EActivation::Activate);

        auto it = mImpl->bodyPoses.find(handle.id);
        if(it != mImpl->bodyPoses.end())
        {
            it->second.prevPosition = position;
            it->second.currPosition = position;
            it->second.prevRotation = rotation;
            it->second.currRotation = rotation;
        }
    }

    void JoltPhysicsWorld::GetTransform(PhysicsBodyHandle handle, glm::vec3 &position,
                                        glm::quat &rotation) const
    {
        if(!handle.IsValid())
        {
            return;
        }

        const JPH::BodyID id(handle.id);
        auto &bodyInterface = mImpl->physicsSystem.GetBodyInterface();
        JPH::RVec3 pos;
        JPH::Quat rot;
        bodyInterface.GetPositionAndRotation(id, pos, rot);
        position = ToGlmVec(pos);
        rotation = ToGlmQuat(rot);
    }

    bool JoltPhysicsWorld::GetInterpolatedBodyPose(PhysicsBodyHandle handle, float alpha,
                                                   glm::vec3 &position, glm::quat &rotation) const
    {
        if(!handle.IsValid())
        {
            return false;
        }

        const auto it = mImpl->bodyPoses.find(handle.id);
        if(it == mImpl->bodyPoses.end())
        {
            return false;
        }

        const float t = std::clamp(alpha, 0.0f, 1.0f);
        position      = glm::mix(it->second.prevPosition, it->second.currPosition, t);
        rotation      = glm::slerp(it->second.prevRotation, it->second.currRotation, t);
        return true;
    }

    void JoltPhysicsWorld::SetLinearVelocity(PhysicsBodyHandle handle, const glm::vec3 &velocity)
    {
        if(!handle.IsValid())
        {
            return;
        }

        const JPH::BodyID id(handle.id);
        auto &bodyInterface = mImpl->physicsSystem.GetBodyInterface();
        bodyInterface.SetLinearVelocity(id, JPH::Vec3(velocity.x, velocity.y, velocity.z));
    }

    glm::vec3 JoltPhysicsWorld::GetLinearVelocity(PhysicsBodyHandle handle) const
    {
        if(!handle.IsValid())
        {
            return {};
        }

        const JPH::BodyID id(handle.id);
        auto &bodyInterface = mImpl->physicsSystem.GetBodyInterface();
        return ToGlmVec(bodyInterface.GetLinearVelocity(id));
    }

    void JoltPhysicsWorld::SetAngularVelocity(PhysicsBodyHandle handle, const glm::vec3 &velocity)
    {
        if(!handle.IsValid())
        {
            return;
        }

        const JPH::BodyID id(handle.id);
        auto &bodyInterface = mImpl->physicsSystem.GetBodyInterface();
        bodyInterface.SetAngularVelocity(id, JPH::Vec3(velocity.x, velocity.y, velocity.z));
    }

    glm::vec3 JoltPhysicsWorld::GetAngularVelocity(PhysicsBodyHandle handle) const
    {
        if(!handle.IsValid())
        {
            return {};
        }

        const JPH::BodyID id(handle.id);
        auto &bodyInterface = mImpl->physicsSystem.GetBodyInterface();
        return ToGlmVec(bodyInterface.GetAngularVelocity(id));
    }

    void JoltPhysicsWorld::AddImpulse(PhysicsBodyHandle handle, const glm::vec3 &impulse)
    {
        if(!handle.IsValid())
        {
            return;
        }

        const JPH::BodyID id(handle.id);
        auto &bodyInterface = mImpl->physicsSystem.GetBodyInterface();
        bodyInterface.AddImpulse(id, JPH::Vec3(impulse.x, impulse.y, impulse.z));
    }

    void JoltPhysicsWorld::AddForce(PhysicsBodyHandle handle, const glm::vec3 &force)
    {
        if(!handle.IsValid())
        {
            return;
        }

        const JPH::BodyID id(handle.id);
        auto &bodyInterface = mImpl->physicsSystem.GetBodyInterface();
        bodyInterface.AddForce(id, JPH::Vec3(force.x, force.y, force.z));
    }

    void JoltPhysicsWorld::AddTorque(PhysicsBodyHandle handle, const glm::vec3 &torque)
    {
        if(!handle.IsValid())
        {
            return;
        }

        const JPH::BodyID id(handle.id);
        auto &bodyInterface = mImpl->physicsSystem.GetBodyInterface();
        bodyInterface.AddTorque(id, JPH::Vec3(torque.x, torque.y, torque.z));
    }

    void JoltPhysicsWorld::AddAngularImpulse(PhysicsBodyHandle handle, const glm::vec3 &impulse)
    {
        if(!handle.IsValid())
        {
            return;
        }

        const JPH::BodyID id(handle.id);
        auto &bodyInterface = mImpl->physicsSystem.GetBodyInterface();
        bodyInterface.AddAngularImpulse(id, JPH::Vec3(impulse.x, impulse.y, impulse.z));
    }

    PhysicsCharacterHandle JoltPhysicsWorld::CreateCharacter(const PhysicsCharacterDesc &desc)
    {
        using namespace JPH;

        const auto layer =
            static_cast<ObjectLayer>(std::min<std::uint8_t>(desc.collisionLayer, kLayerCount - 1));
        mImpl->NoteLayer(desc.collisionLayer, desc.collideWithLayers, true);

        const float radius = std::max(desc.radius, 0.001f);
        RefConst<Shape> standingShape =
            MakeStandingCapsule(desc.radius, desc.height, desc.centerOffset);

        Ref<CharacterVirtualSettings> settings = new CharacterVirtualSettings();
        settings->mMass                        = std::max(desc.mass, 0.001f);
        settings->mMaxStrength                 = std::max(desc.maxStrength, 0.0f);
        settings->mMaxSlopeAngle =
            JPH::DegreesToRadians(std::clamp(desc.maxSlopeDegrees, 1.0f, 89.0f));
        settings->mShape                       = standingShape;
        settings->mSupportingVolume            = Plane(Vec3::sAxisY(), -radius);
        settings->mPredictiveContactDistance   = std::max(desc.predictiveContactDistance, 0.0f);
        settings->mCharacterPadding            = std::max(desc.characterPadding, 0.0f);
        settings->mPenetrationRecoverySpeed =
            std::clamp(desc.penetrationRecoverySpeed, 0.0f, 1.0f);
        settings->mEnhancedInternalEdgeRemoval = desc.enhancedInternalEdgeRemoval;

        const RVec3 position(desc.position.x, desc.position.y, desc.position.z);
        const Quat rotation(desc.rotation.x, desc.rotation.y, desc.rotation.z, desc.rotation.w);

        Ref<CharacterVirtual> character =
            new CharacterVirtual(settings, position, rotation, 0, &mImpl->physicsSystem);

        const std::uint32_t id = mImpl->nextCharacterId++;
        mImpl->characters.emplace(
            id, CharacterEntry {.character            = character,
                                .layer                = layer,
                                .stickToFloorDistance = std::max(desc.stickToFloorDistance, 0.0f),
                                .walkStairsStepHeight = std::max(desc.walkStairsStepHeight, 0.0f),
                                .entityId             = 0,
                                .prevPosition         = desc.position,
                                .currPosition         = desc.position});
        return PhysicsCharacterHandle {.id = id};
    }

    void JoltPhysicsWorld::DestroyCharacter(PhysicsCharacterHandle handle)
    {
        if(!handle.IsValid())
        {
            return;
        }

        const auto it = mImpl->characters.find(handle.id);
        if(it != mImpl->characters.end() && it->second.character != nullptr)
        {
            it->second.character->SetListener(nullptr);
        }

        mImpl->characters.erase(handle.id);
        for(auto itEntity = mImpl->entityCharacters.begin();
            itEntity != mImpl->entityCharacters.end();)
        {
            if(itEntity->second.id == handle.id)
            {
                itEntity = mImpl->entityCharacters.erase(itEntity);
            }
            else
            {
                ++itEntity;
            }
        }
    }

    void JoltPhysicsWorld::BindCharacter(std::uint64_t entity, PhysicsCharacterHandle handle)
    {
        if(!handle.IsValid())
        {
            UnbindCharacter(entity);
            return;
        }

        const auto it = mImpl->characters.find(handle.id);
        if(it == mImpl->characters.end() || it->second.character == nullptr)
        {
            return;
        }

        // Drop any previous binding of this handle to another entity.
        for(auto itEntity = mImpl->entityCharacters.begin();
            itEntity != mImpl->entityCharacters.end();)
        {
            if(itEntity->second.id == handle.id && itEntity->first != entity)
            {
                itEntity = mImpl->entityCharacters.erase(itEntity);
            }
            else
            {
                ++itEntity;
            }
        }

        it->second.entityId = entity;
        it->second.character->SetUserData(entity);
        it->second.character->SetListener(&mImpl->characterContactListener);
        mImpl->entityCharacters[entity] = handle;
    }

    void JoltPhysicsWorld::UnbindCharacter(std::uint64_t entity)
    {
        const auto bound = mImpl->entityCharacters.find(entity);
        if(bound != mImpl->entityCharacters.end())
        {
            const auto it = mImpl->characters.find(bound->second.id);
            if(it != mImpl->characters.end() && it->second.character != nullptr)
            {
                it->second.entityId = 0;
                it->second.character->SetUserData(0);
                it->second.character->SetListener(nullptr);
            }
            mImpl->entityCharacters.erase(bound);
        }
    }

    PhysicsCharacterHandle JoltPhysicsWorld::FindCharacter(std::uint64_t entity) const
    {
        const auto it = mImpl->entityCharacters.find(entity);
        if(it == mImpl->entityCharacters.end())
        {
            return {};
        }
        return it->second;
    }

    void JoltPhysicsWorld::ForEachCharacter(
        const std::function<void(std::uint64_t, PhysicsCharacterHandle)> &visit) const
    {
        if(!visit)
        {
            return;
        }

        std::vector<std::pair<std::uint64_t, PhysicsCharacterHandle>> snapshot;
        snapshot.reserve(mImpl->entityCharacters.size());

        for(const auto &[entity, handle]: mImpl->entityCharacters)
        {
            snapshot.emplace_back(entity, handle);
        }

        for(const auto &[entity, handle]: snapshot)
        {
            visit(entity, handle);
        }
    }

    void JoltPhysicsWorld::SetCharacterVelocity(PhysicsCharacterHandle handle,
                                                const glm::vec3 &velocity)
    {
        if(!handle.IsValid())
        {
            return;
        }
        const auto it = mImpl->characters.find(handle.id);
        if(it == mImpl->characters.end() || it->second.character == nullptr)
        {
            return;
        }
        it->second.character->SetLinearVelocity(JPH::Vec3(velocity.x, velocity.y, velocity.z));
    }

    glm::vec3 JoltPhysicsWorld::GetCharacterVelocity(PhysicsCharacterHandle handle) const
    {
        if(!handle.IsValid())
        {
            return {};
        }
        const auto it = mImpl->characters.find(handle.id);
        if(it == mImpl->characters.end() || it->second.character == nullptr)
        {
            return {};
        }
        return ToGlmVec(it->second.character->GetLinearVelocity());
    }

    void JoltPhysicsWorld::GetCharacterTransform(PhysicsCharacterHandle handle, glm::vec3 &position,
                                                 glm::quat &rotation) const
    {
        if(!handle.IsValid())
        {
            return;
        }
        const auto it = mImpl->characters.find(handle.id);
        if(it == mImpl->characters.end() || it->second.character == nullptr)
        {
            return;
        }
        position = ToGlmVec(it->second.character->GetPosition());
        rotation = ToGlmQuat(it->second.character->GetRotation());
    }

    bool JoltPhysicsWorld::GetInterpolatedCharacterPosition(PhysicsCharacterHandle handle,
                                                            float alpha, glm::vec3 &position) const
    {
        if(!handle.IsValid())
        {
            return false;
        }
        const auto it = mImpl->characters.find(handle.id);
        if(it == mImpl->characters.end() || it->second.character == nullptr)
        {
            return false;
        }

        const float t = std::clamp(alpha, 0.0f, 1.0f);
        position      = glm::mix(it->second.prevPosition, it->second.currPosition, t);
        return true;
    }

    void JoltPhysicsWorld::SetCharacterPosition(PhysicsCharacterHandle handle,
                                                const glm::vec3 &position)
    {
        if(!handle.IsValid())
        {
            return;
        }
        const auto it = mImpl->characters.find(handle.id);
        if(it == mImpl->characters.end() || it->second.character == nullptr)
        {
            return;
        }

        using namespace JPH;
        auto &entry = it->second;
        entry.character->SetPosition(RVec3(position.x, position.y, position.z));
        entry.prevPosition = position;
        entry.currPosition = position;
        entry.character->RefreshContacts(
            mImpl->physicsSystem.GetDefaultBroadPhaseLayerFilter(entry.layer),
            mImpl->physicsSystem.GetDefaultLayerFilter(entry.layer), {}, {}, mImpl->tempAllocator);
    }

    void JoltPhysicsWorld::SetCharacterRotation(PhysicsCharacterHandle handle,
                                                const glm::quat &rotation)
    {
        if(!handle.IsValid())
        {
            return;
        }
        const auto it = mImpl->characters.find(handle.id);
        if(it == mImpl->characters.end() || it->second.character == nullptr)
        {
            return;
        }
        it->second.character->SetRotation(
            JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w));
    }

    bool JoltPhysicsWorld::IsCharacterGrounded(PhysicsCharacterHandle handle) const
    {
        return GetCharacterGroundInfo(handle).grounded;
    }

    CharacterGroundInfo JoltPhysicsWorld::GetCharacterGroundInfo(
        PhysicsCharacterHandle handle) const
    {
        CharacterGroundInfo info {};
        if(!handle.IsValid())
        {
            return info;
        }
        const auto it = mImpl->characters.find(handle.id);
        if(it == mImpl->characters.end() || it->second.character == nullptr)
        {
            return info;
        }

        const auto *character = it->second.character.GetPtr();
        switch(character->GetGroundState())
        {
        case JPH::CharacterBase::EGroundState::OnGround:
            info.state = CharacterGroundState::OnGround;
            break;
        case JPH::CharacterBase::EGroundState::OnSteepGround:
            info.state = CharacterGroundState::OnSteepGround;
            break;
        case JPH::CharacterBase::EGroundState::NotSupported:
            info.state = CharacterGroundState::NotSupported;
            break;
        case JPH::CharacterBase::EGroundState::InAir:
        default:
            info.state = CharacterGroundState::InAir;
            break;
        }

        info.grounded  = character->IsSupported();
        info.position  = ToGlmVec(character->GetGroundPosition());
        info.normal    = ToGlmVec(character->GetGroundNormal());
        info.velocity  = ToGlmVec(character->GetGroundVelocity());

        const JPH::BodyID groundId = character->GetGroundBodyID();
        if(!groundId.IsInvalid())
        {
            info.groundBody = PhysicsBodyHandle {.id = groundId.GetIndexAndSequenceNumber()};
        }
        return info;
    }

    bool JoltPhysicsWorld::SetCharacterShape(PhysicsCharacterHandle handle,
                                             const PhysicsCharacterShapeDesc &shape)
    {
        if(!handle.IsValid())
        {
            return false;
        }
        const auto it = mImpl->characters.find(handle.id);
        if(it == mImpl->characters.end() || it->second.character == nullptr)
        {
            return false;
        }

        using namespace JPH;
        auto &entry = it->second;
        RefConst<Shape> standingShape =
            MakeStandingCapsule(shape.radius, shape.height, shape.centerOffset);
        constexpr float kMaxPenetration = 0.1f;
        return entry.character->SetShape(
            standingShape, kMaxPenetration,
            mImpl->physicsSystem.GetDefaultBroadPhaseLayerFilter(entry.layer),
            mImpl->physicsSystem.GetDefaultLayerFilter(entry.layer), {}, {}, mImpl->tempAllocator);
    }

    void JoltPhysicsWorld::SetCharacterMaxStrength(PhysicsCharacterHandle handle, float maxStrength)
    {
        if(!handle.IsValid())
        {
            return;
        }
        const auto it = mImpl->characters.find(handle.id);
        if(it == mImpl->characters.end() || it->second.character == nullptr)
        {
            return;
        }
        it->second.character->SetMaxStrength(std::max(maxStrength, 0.0f));
    }

    PhysicsJointHandle JoltPhysicsWorld::CreateJoint(const PhysicsJointDesc &desc)
    {
        using namespace JPH;

        if(!desc.bodyA.IsValid() || !desc.bodyB.IsValid())
        {
            return {};
        }

        const BodyID idA(desc.bodyA.id);
        const BodyID idB(desc.bodyB.id);
        BodyInterface &bodyInterface = mImpl->physicsSystem.GetBodyInterface();
        if(!bodyInterface.IsAdded(idA) || !bodyInterface.IsAdded(idB))
        {
            return {};
        }

        TwoBodyConstraint *constraint = nullptr;
        switch(desc.type)
        {
        case PhysicsJointType::Fixed: {
            FixedConstraintSettings settings;
            settings.mSpace           = EConstraintSpace::LocalToBodyCOM;
            settings.mAutoDetectPoint = false;
            settings.mPoint1 =
                RVec3(desc.localAnchorA.x, desc.localAnchorA.y, desc.localAnchorA.z);
            settings.mPoint2 =
                RVec3(desc.localAnchorB.x, desc.localAnchorB.y, desc.localAnchorB.z);
            constraint = bodyInterface.CreateConstraint(&settings, idA, idB);
            break;
        }
        case PhysicsJointType::Hinge: {
            HingeConstraintSettings settings;
            settings.mSpace = EConstraintSpace::LocalToBodyCOM;
            settings.mPoint1 =
                RVec3(desc.localAnchorA.x, desc.localAnchorA.y, desc.localAnchorA.z);
            settings.mPoint2 =
                RVec3(desc.localAnchorB.x, desc.localAnchorB.y, desc.localAnchorB.z);
            Vec3 hingeAxis(desc.hingeAxisLocalA.x, desc.hingeAxisLocalA.y, desc.hingeAxisLocalA.z);
            if(hingeAxis.LengthSq() < 1.0e-8f)
            {
                hingeAxis = Vec3::sAxisY();
            }
            hingeAxis              = hingeAxis.Normalized();
            const Vec3 normalAxis  = hingeAxis.GetNormalizedPerpendicular();
            settings.mHingeAxis1   = hingeAxis;
            settings.mHingeAxis2   = hingeAxis;
            settings.mNormalAxis1  = normalAxis;
            settings.mNormalAxis2  = normalAxis;
            constraint             = bodyInterface.CreateConstraint(&settings, idA, idB);
            break;
        }
        }

        if(constraint == nullptr)
        {
            return {};
        }

        mImpl->physicsSystem.AddConstraint(constraint);
        const std::uint32_t id = mImpl->nextJointId++;
        mImpl->joints.emplace(id, constraint);
        return PhysicsJointHandle {.id = id};
    }

    void JoltPhysicsWorld::DestroyJoint(PhysicsJointHandle handle)
    {
        if(!handle.IsValid())
        {
            return;
        }

        const auto it = mImpl->joints.find(handle.id);
        if(it == mImpl->joints.end())
        {
            return;
        }

        if(it->second != nullptr)
        {
            mImpl->physicsSystem.RemoveConstraint(it->second);
        }
        mImpl->joints.erase(it);
    }

    namespace
    {
        struct QueryFilters
        {
            MaskObjectLayerFilter         objectFilter;
            AcceptAllBroadPhaseLayerFilter broadPhaseFilter;
            IgnoreEntityBodyFilter         bodyFilter;

            explicit QueryFilters(const QueryFilter &filter)
                : objectFilter(filter.collideWithLayers), bodyFilter(filter.ignoreEntity)
            {
            }
        };

        [[nodiscard]] RaycastHit MakeMiss()
        {
            return {};
        }

        [[nodiscard]] RaycastHit FillHitFromBody(JPH::PhysicsSystem &system, JPH::BodyID bodyId,
                                                 const glm::vec3 &point, const glm::vec3 &normal,
                                                 float fraction, float distance)
        {
            RaycastHit hit;
            hit.hit      = true;
            hit.point    = point;
            hit.normal   = normal;
            hit.fraction = fraction;
            hit.distance = distance;
            hit.body     = PhysicsBodyHandle {.id = bodyId.GetIndexAndSequenceNumber()};
            hit.entityId = static_cast<std::uint64_t>(system.GetBodyInterface().GetUserData(bodyId));
            return hit;
        }
    } // namespace

    RaycastHit JoltPhysicsWorld::Raycast(const glm::vec3 &origin, const glm::vec3 &direction,
                                         float maxDistance, const QueryFilter &filter) const
    {
        using namespace JPH;

        if(maxDistance <= 0.0f)
        {
            return MakeMiss();
        }

        glm::vec3 dir = direction;
        const float dirLenSq = glm::dot(dir, dir);
        if(dirLenSq < 1.0e-12f)
        {
            return MakeMiss();
        }
        dir /= std::sqrt(dirLenSq);

        QueryFilters filters(filter);
        const RRayCast ray(RVec3(origin.x, origin.y, origin.z),
                           Vec3(dir.x, dir.y, dir.z) * maxDistance);

        RayCastResult castHit;
        const bool hit = mImpl->physicsSystem.GetNarrowPhaseQuery().CastRay(
            ray, castHit, filters.broadPhaseFilter, filters.objectFilter, filters.bodyFilter);
        if(!hit)
        {
            return MakeMiss();
        }

        const RVec3 hitPoint = ray.GetPointOnRay(castHit.mFraction);
        Vec3        normal   = Vec3::sAxisY();

        const BodyLockInterface &lockInterface = mImpl->physicsSystem.GetBodyLockInterface();
        {
            BodyLockRead lock(lockInterface, castHit.mBodyID);
            if(lock.Succeeded())
            {
                normal = lock.GetBody().GetWorldSpaceSurfaceNormal(castHit.mSubShapeID2, hitPoint);
            }
        }

        return FillHitFromBody(mImpl->physicsSystem, castHit.mBodyID, ToGlmVec(hitPoint), ToGlmVec(normal),
                               castHit.mFraction, castHit.mFraction * maxDistance);
    }

    RaycastHit JoltPhysicsWorld::SphereCast(const glm::vec3 &origin, const glm::vec3 &direction,
                                            float radius, float maxDistance,
                                            const QueryFilter &filter) const
    {
        using namespace JPH;

        if(maxDistance <= 0.0f)
        {
            return MakeMiss();
        }

        glm::vec3 dir = direction;
        const float dirLenSq = glm::dot(dir, dir);
        if(dirLenSq < 1.0e-12f)
        {
            return MakeMiss();
        }
        dir /= std::sqrt(dirLenSq);

        RefConst<Shape> shape = new SphereShape(std::max(radius, 0.001f));
        const RMat44 start =
            RMat44::sTranslation(RVec3(origin.x, origin.y, origin.z));
        const RShapeCast shapeCast = RShapeCast::sFromWorldTransform(
            shape, Vec3::sOne(), start, Vec3(dir.x, dir.y, dir.z) * maxDistance);

        QueryFilters filters(filter);
        ShapeCastSettings settings;
        ClosestHitCollisionCollector<CastShapeCollector> collector;
        mImpl->physicsSystem.GetNarrowPhaseQuery().CastShape(
            shapeCast, settings, RVec3::sZero(), collector, filters.broadPhaseFilter,
            filters.objectFilter, filters.bodyFilter);

        if(!collector.HadHit())
        {
            return MakeMiss();
        }

        const ShapeCastResult &result = collector.mHit;
        Vec3 normal = result.mPenetrationAxis;
        if(normal.LengthSq() > 1.0e-12f)
        {
            normal = -normal.Normalized();
        }
        else
        {
            normal = Vec3::sAxisY();
        }

        return FillHitFromBody(mImpl->physicsSystem, result.mBodyID2, ToGlmVec(result.mContactPointOn2),
                               ToGlmVec(normal), result.mFraction, result.mFraction * maxDistance);
    }

    RaycastHit JoltPhysicsWorld::CapsuleCast(const glm::vec3 &origin, const glm::vec3 &direction,
                                             float radius, float height, float maxDistance,
                                             const QueryFilter &filter) const
    {
        using namespace JPH;

        if(maxDistance <= 0.0f)
        {
            return MakeMiss();
        }

        glm::vec3 dir = direction;
        const float dirLenSq = glm::dot(dir, dir);
        if(dirLenSq < 1.0e-12f)
        {
            return MakeMiss();
        }
        dir /= std::sqrt(dirLenSq);

        const float r          = std::max(radius, 0.001f);
        const float halfHeight = std::max(0.5f * height, 0.001f);
        RefConst<Shape> shape  = new CapsuleShape(halfHeight, r);
        const RMat44 start =
            RMat44::sTranslation(RVec3(origin.x, origin.y, origin.z));
        const RShapeCast shapeCast = RShapeCast::sFromWorldTransform(
            shape, Vec3::sOne(), start, Vec3(dir.x, dir.y, dir.z) * maxDistance);

        QueryFilters filters(filter);
        ShapeCastSettings settings;
        ClosestHitCollisionCollector<CastShapeCollector> collector;
        mImpl->physicsSystem.GetNarrowPhaseQuery().CastShape(
            shapeCast, settings, RVec3::sZero(), collector, filters.broadPhaseFilter,
            filters.objectFilter, filters.bodyFilter);

        if(!collector.HadHit())
        {
            return MakeMiss();
        }

        const ShapeCastResult &result = collector.mHit;
        Vec3 normal = result.mPenetrationAxis;
        if(normal.LengthSq() > 1.0e-12f)
        {
            normal = -normal.Normalized();
        }
        else
        {
            normal = Vec3::sAxisY();
        }

        return FillHitFromBody(mImpl->physicsSystem, result.mBodyID2, ToGlmVec(result.mContactPointOn2),
                               ToGlmVec(normal), result.mFraction, result.mFraction * maxDistance);
    }

    std::vector<OverlapHit> JoltPhysicsWorld::OverlapSphere(const glm::vec3 &center, float radius,
                                                            const QueryFilter &filter) const
    {
        using namespace JPH;

        std::vector<OverlapHit> hits;
        RefConst<Shape> shape = new SphereShape(std::max(radius, 0.001f));
        const RMat44 com =
            RMat44::sTranslation(RVec3(center.x, center.y, center.z)).PreTranslated(shape->GetCenterOfMass());

        QueryFilters filters(filter);
        CollideShapeSettings settings;
        AllHitCollisionCollector<CollideShapeCollector> collector;
        mImpl->physicsSystem.GetNarrowPhaseQuery().CollideShape(
            shape, Vec3::sOne(), com, settings, RVec3::sZero(), collector, filters.broadPhaseFilter,
            filters.objectFilter, filters.bodyFilter);

        hits.reserve(collector.mHits.size());
        auto &bodyInterface = mImpl->physicsSystem.GetBodyInterface();
        for(const CollideShapeResult &result: collector.mHits)
        {
            OverlapHit hit;
            hit.body     = PhysicsBodyHandle {.id = result.mBodyID2.GetIndexAndSequenceNumber()};
            hit.entityId = static_cast<std::uint64_t>(bodyInterface.GetUserData(result.mBodyID2));
            hits.push_back(hit);
        }
        return hits;
    }

    std::vector<OverlapHit> JoltPhysicsWorld::OverlapBox(const glm::vec3 &center,
                                                         const glm::vec3 &halfExtents,
                                                         const glm::quat &rotation,
                                                         const QueryFilter &filter) const
    {
        using namespace JPH;

        std::vector<OverlapHit> hits;
        const Vec3 half {std::max(halfExtents.x, 0.001f), std::max(halfExtents.y, 0.001f),
                         std::max(halfExtents.z, 0.001f)};
        RefConst<Shape> shape = new BoxShape(half);
        const RMat44 world =
            RMat44::sRotationTranslation(Quat(rotation.x, rotation.y, rotation.z, rotation.w),
                                         RVec3(center.x, center.y, center.z));
        const RMat44 com = world.PreTranslated(shape->GetCenterOfMass());

        QueryFilters filters(filter);
        CollideShapeSettings settings;
        AllHitCollisionCollector<CollideShapeCollector> collector;
        mImpl->physicsSystem.GetNarrowPhaseQuery().CollideShape(
            shape, Vec3::sOne(), com, settings, RVec3::sZero(), collector, filters.broadPhaseFilter,
            filters.objectFilter, filters.bodyFilter);

        hits.reserve(collector.mHits.size());
        auto &bodyInterface = mImpl->physicsSystem.GetBodyInterface();
        for(const CollideShapeResult &result: collector.mHits)
        {
            OverlapHit hit;
            hit.body     = PhysicsBodyHandle {.id = result.mBodyID2.GetIndexAndSequenceNumber()};
            hit.entityId = static_cast<std::uint64_t>(bodyInterface.GetUserData(result.mBodyID2));
            hits.push_back(hit);
        }
        return hits;
    }

    std::vector<TriggerEvent> JoltPhysicsWorld::DrainTriggerEvents()
    {
        std::lock_guard lock(mImpl->eventMutex);
        std::vector<TriggerEvent> events;
        events.swap(mImpl->readyTriggers);
        return events;
    }

    std::vector<PhysicsContactEvent> JoltPhysicsWorld::DrainContactEvents()
    {
        std::lock_guard lock(mImpl->eventMutex);
        std::vector<PhysicsContactEvent> events;
        events.swap(mImpl->readyContacts);
        return events;
    }

    void JoltPhysicsWorld::SetGravity(const glm::vec3 &gravity)
    {
        mImpl->physicsSystem.SetGravity(JPH::Vec3(gravity.x, gravity.y, gravity.z));
    }

    glm::vec3 JoltPhysicsWorld::GetGravity() const
    {
        return ToGlmVec(mImpl->physicsSystem.GetGravity());
    }

    bool JoltPhysicsWorld::IsBodyActive(PhysicsBodyHandle handle) const
    {
        if(!handle.IsValid())
        {
            return false;
        }

        const JPH::BodyID id(handle.id);
        auto &bodyInterface = mImpl->physicsSystem.GetBodyInterface();
        if(!bodyInterface.IsAdded(id))
        {
            return false;
        }
        return bodyInterface.IsActive(id);
    }

} // namespace FRIGGA_NAMESPACE
