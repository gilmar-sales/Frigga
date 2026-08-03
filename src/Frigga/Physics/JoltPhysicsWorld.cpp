#include "JoltPhysicsWorld.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <thread>

JPH_SUPPRESS_WARNINGS

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        constexpr unsigned kMaxBodies             = 65536;
        constexpr unsigned kMaxBodyPairs          = 65536;
        constexpr unsigned kMaxContactConstraints = 20480;
        constexpr unsigned kLayerCount            = 16;

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
            [[nodiscard]] const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
            {
                switch((JPH::BroadPhaseLayer::Type)layer)
                {
                case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NonMoving:
                    return "NON_MOVING";
                case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::Moving:
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
                const bool moving =
                    layer < kLayerCount ? mLayerIsMoving[layer] : true;
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
            layerMasks.fill(0xffff);
            layerIsMoving.fill(false);

            physicsSystem.Init(kMaxBodies, 0, kMaxBodyPairs, kMaxContactConstraints, broadPhase,
                               objectVsBroadphase, objectVsObject);
            physicsSystem.SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));
        }

        JPH::TempAllocatorImpl tempAllocator;
        JPH::JobSystemThreadPool jobSystem;
        std::array<bool, kLayerCount> layerIsMoving {};
        std::array<std::uint16_t, kLayerCount> layerMasks {};
        BPLayerInterfaceImpl broadPhase;
        ObjectVsBroadPhaseLayerFilterImpl objectVsBroadphase;
        ObjectLayerPairFilterImpl objectVsObject;
        JPH::PhysicsSystem physicsSystem;
        float accumulator = 0.0f;
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
        auto &bodyInterface = mImpl->physicsSystem.GetBodyInterface();
        JPH::BodyIDVector bodies;
        mImpl->physicsSystem.GetBodies(bodies);
        for(const JPH::BodyID id : bodies)
        {
            bodyInterface.RemoveBody(id);
            bodyInterface.DestroyBody(id);
        }
        mImpl->layerMasks.fill(0xffff);
        mImpl->layerIsMoving.fill(false);
        mImpl->accumulator = 0.0f;
    }

    void JoltPhysicsWorld::OptimizeBroadPhase()
    {
        mImpl->physicsSystem.OptimizeBroadPhase();
    }

    void JoltPhysicsWorld::Step(float deltaTime)
    {
        constexpr float fixedDt = 1.0f / 60.0f;
        mImpl->accumulator += deltaTime;
        // Avoid spiral of death after stalls.
        mImpl->accumulator = std::min(mImpl->accumulator, fixedDt * 5.0f);

        while(mImpl->accumulator >= fixedDt)
        {
            const int collisionSteps = 1;
            mImpl->physicsSystem.Update(fixedDt, collisionSteps, &mImpl->tempAllocator,
                                        &mImpl->jobSystem);
            mImpl->accumulator -= fixedDt;
        }
    }

    PhysicsBodyHandle JoltPhysicsWorld::CreateBody(const PhysicsBodyDesc &desc)
    {
        using namespace JPH;

        const auto layer =
            static_cast<ObjectLayer>(std::min<std::uint8_t>(desc.collisionLayer, kLayerCount - 1));
        mImpl->layerMasks[layer] = desc.collideWithLayers;
        mImpl->layerIsMoving[layer] =
            desc.motion == BodyMotionType::Dynamic || desc.motion == BodyMotionType::Kinematic;

        RefConst<Shape> shape;
        switch(desc.shape)
        {
        case ColliderShape::Box:
        {
            const Vec3 half {std::max(desc.halfExtents.x * desc.scale.x, 0.001f),
                             std::max(desc.halfExtents.y * desc.scale.y, 0.001f),
                             std::max(desc.halfExtents.z * desc.scale.z, 0.001f)};
            shape = new BoxShape(half);
            break;
        }
        case ColliderShape::Sphere:
        {
            const float radius =
                std::max(desc.radius * std::max({desc.scale.x, desc.scale.y, desc.scale.z}), 0.001f);
            shape = new SphereShape(radius);
            break;
        }
        case ColliderShape::Capsule:
        {
            const float radius =
                std::max(desc.radius * std::max(desc.scale.x, desc.scale.z), 0.001f);
            const float halfHeight = std::max(0.5f * desc.height * desc.scale.y, 0.001f);
            shape                  = new CapsuleShape(halfHeight, radius);
            break;
        }
        case ColliderShape::Mesh:
        {
            Array<Vec3> points;
            points.reserve(desc.meshPoints.size());
            for(const auto &p : desc.meshPoints)
            {
                points.push_back(Vec3(p.x * desc.scale.x, p.y * desc.scale.y, p.z * desc.scale.z));
            }
            if(points.size() < 3)
            {
                // Fallback unit box if hull cannot be built.
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
            settings.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = std::max(desc.mass, 0.001f);
        }

        BodyInterface &bodyInterface = mImpl->physicsSystem.GetBodyInterface();
        const BodyID id =
            bodyInterface.CreateAndAddBody(settings, desc.motion == BodyMotionType::Static
                                                         ? EActivation::DontActivate
                                                         : EActivation::Activate);
        if(id.IsInvalid())
        {
            return {};
        }

        return PhysicsBodyHandle {.id = id.GetIndexAndSequenceNumber()};
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

    void JoltPhysicsWorld::SetGravity(const glm::vec3 &gravity)
    {
        mImpl->physicsSystem.SetGravity(JPH::Vec3(gravity.x, gravity.y, gravity.z));
    }

    glm::vec3 JoltPhysicsWorld::GetGravity() const
    {
        const auto g = mImpl->physicsSystem.GetGravity();
        return {g.GetX(), g.GetY(), g.GetZ()};
    }

} // namespace FRIGGA_NAMESPACE
