#include "JoltPhysicsWorld.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

JPH_SUPPRESS_WARNINGS

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        constexpr unsigned kMaxBodies             = 65536;
        constexpr unsigned kMaxBodyPairs          = 65536;
        constexpr unsigned kMaxContactConstraints = 20480;
        constexpr unsigned kLayerCount            = 16;
        constexpr float kFixedDeltaTime           = 1.0f / 60.0f;

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
            constexpr JPH::BroadPhaseLayer NonMoving{0};
            constexpr JPH::BroadPhaseLayer Moving{1};
            constexpr unsigned NumLayers = 2;
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
            JPH::ObjectLayer layer = 0;
        };
    } // namespace

    struct JoltPhysicsWorld::Impl
    {
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

        JPH::TempAllocatorImpl tempAllocator;
        JPH::JobSystemThreadPool jobSystem;
        std::array<bool, kLayerCount> layerIsMoving{};
        std::array<std::uint16_t, kLayerCount> layerMasks{};
        std::array<std::uint16_t, kLayerCount> layerBodyCount{};
        BPLayerInterfaceImpl broadPhase;
        ObjectVsBroadPhaseLayerFilterImpl objectVsBroadphase;
        ObjectLayerPairFilterImpl objectVsObject;
        JPH::PhysicsSystem physicsSystem;
        float accumulator = 0.0f;
        std::unordered_map<std::uint32_t, CharacterEntry> characters;
        std::unordered_map<std::uint64_t, PhysicsCharacterHandle> entityCharacters;
        std::uint32_t nextCharacterId = 1;
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
        mImpl->characters.clear();
        mImpl->entityCharacters.clear();

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
        mImpl->accumulator     = 0.0f;
        mImpl->nextCharacterId = 1;
    }

    void JoltPhysicsWorld::OptimizeBroadPhase()
    {
        mImpl->physicsSystem.OptimizeBroadPhase();
    }

    float JoltPhysicsWorld::GetFixedDeltaTime() const
    {
        return kFixedDeltaTime;
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
            entry.character->ExtendedUpdate(
                kFixedDeltaTime, worldGravity, updateSettings,
                mImpl->physicsSystem.GetDefaultBroadPhaseLayerFilter(entry.layer),
                mImpl->physicsSystem.GetDefaultLayerFilter(entry.layer), {}, {},
                mImpl->tempAllocator);
        }
    }

    void JoltPhysicsWorld::stepFixedInternal(int steps)
    {
        const int count = std::max(steps, 0);
        for(int i = 0; i < count; ++i)
        {
            mImpl->physicsSystem.Update(kFixedDeltaTime, 1, &mImpl->tempAllocator,
                                        &mImpl->jobSystem);
            updateCharactersFixed();
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
    }

    void JoltPhysicsWorld::StepFixed(int steps)
    {
        mImpl->accumulator = 0.0f;
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
            const Vec3 half{std::max(desc.halfExtents.x * desc.scale.x, 0.001f),
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

        return PhysicsBodyHandle{.id = id.GetIndexAndSequenceNumber()};
    }

    void JoltPhysicsWorld::DestroyBody(PhysicsBodyHandle handle)
    {
        if(!handle.IsValid())
        {
            return;
        }

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
        position = {pos.GetX(), pos.GetY(), pos.GetZ()};
        rotation = {rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ()};
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
        const auto v        = bodyInterface.GetLinearVelocity(id);
        return {v.GetX(), v.GetY(), v.GetZ()};
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

    PhysicsCharacterHandle JoltPhysicsWorld::CreateCharacter(const PhysicsCharacterDesc &desc)
    {
        using namespace JPH;

        const auto layer =
            static_cast<ObjectLayer>(std::min<std::uint8_t>(desc.collisionLayer, kLayerCount - 1));
        mImpl->NoteLayer(desc.collisionLayer, desc.collideWithLayers, true);

        const float radius      = std::max(desc.radius, 0.001f);
        const float halfHeight  = std::max(0.5f * desc.height, 0.001f);
        RefConst<Shape> capsule = new CapsuleShape(halfHeight, radius);
        // Feet at CharacterVirtual position; centerOffset shifts the capsule center further.
        RefConst<Shape> standingShape = new RotatedTranslatedShape(
            Vec3(desc.centerOffset.x, halfHeight + radius + desc.centerOffset.y,
                 desc.centerOffset.z),
            Quat::sIdentity(), capsule);

        Ref<CharacterVirtualSettings> settings = new CharacterVirtualSettings();
        settings->mMass                        = std::max(desc.mass, 0.001f);
        settings->mMaxSlopeAngle =
            JPH::DegreesToRadians(std::clamp(desc.maxSlopeDegrees, 1.0f, 89.0f));
        settings->mShape            = standingShape;
        settings->mSupportingVolume = Plane(Vec3::sAxisY(), -radius);

        const RVec3 position(desc.position.x, desc.position.y, desc.position.z);
        const Quat rotation(desc.rotation.x, desc.rotation.y, desc.rotation.z, desc.rotation.w);

        Ref<CharacterVirtual> character =
            new CharacterVirtual(settings, position, rotation, 0, &mImpl->physicsSystem);

        const std::uint32_t id = mImpl->nextCharacterId++;
        mImpl->characters.emplace(id, CharacterEntry{.character = character, .layer = layer});
        return PhysicsCharacterHandle{.id = id};
    }

    void JoltPhysicsWorld::DestroyCharacter(PhysicsCharacterHandle handle)
    {
        if(!handle.IsValid())
        {
            return;
        }
        mImpl->characters.erase(handle.id);
        for(auto it = mImpl->entityCharacters.begin(); it != mImpl->entityCharacters.end();)
        {
            if(it->second.id == handle.id)
            {
                it = mImpl->entityCharacters.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void JoltPhysicsWorld::BindCharacter(std::uint64_t entity, PhysicsCharacterHandle handle)
    {
        if(!handle.IsValid())
        {
            mImpl->entityCharacters.erase(entity);
            return;
        }
        mImpl->entityCharacters[entity] = handle;
    }

    void JoltPhysicsWorld::UnbindCharacter(std::uint64_t entity)
    {
        mImpl->entityCharacters.erase(entity);
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
        
        for(const auto &[entity, handle] : snapshot)
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
        const auto v = it->second.character->GetLinearVelocity();
        return {v.GetX(), v.GetY(), v.GetZ()};
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
        const auto pos = it->second.character->GetPosition();
        const auto rot = it->second.character->GetRotation();
        position       = {pos.GetX(), pos.GetY(), pos.GetZ()};
        rotation       = {rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ()};
    }

    bool JoltPhysicsWorld::IsCharacterGrounded(PhysicsCharacterHandle handle) const
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
        return it->second.character->IsSupported();
    }

    void JoltPhysicsWorld::SetGravity(const glm::vec3 &gravity)
    {
        mImpl->physicsSystem.SetGravity(JPH::Vec3(gravity.x, gravity.y, gravity.z));
    }

    glm::vec3 JoltPhysicsWorld::GetGravity() const
    {
        const auto g = mImpl->physicsSystem.GetGravity();
        return {g.GetX(), g.GetY(), g.GetZ()};
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
