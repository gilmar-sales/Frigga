#pragma once

#include "Frigga/Macro.hpp"
#include "Frigga/Physics/PhysicsBodyHandle.hpp"

#include <cstdint>
#include <limits>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace FRIGGA_NAMESPACE
{

    enum class BodyMotionType : std::uint8_t
    {
        Static = 0,
        Kinematic,
        Dynamic,
    };

    enum class ColliderShape : std::uint8_t
    {
        Box = 0,
        Sphere,
        Capsule,
        Mesh, // Convex hull cooked from points / primitive mesh
    };

    enum class CharacterGroundState : std::uint8_t
    {
        OnGround = 0,
        OnSteepGround,
        NotSupported,
        InAir,
    };

    enum class TriggerEventType : std::uint8_t
    {
        Enter = 0,
        Stay,
        Exit,
    };

    enum class PhysicsContactKind : std::uint8_t
    {
        BodyBody = 0,
        CharacterBody,
    };

    enum class PhysicsJointType : std::uint8_t
    {
        Fixed = 0,
        Hinge,
    };

    struct PhysicsBodyDesc
    {
        BodyMotionType motion = BodyMotionType::Dynamic;
        ColliderShape  shape  = ColliderShape::Box;

        glm::vec3 position {0.0f};
        glm::quat rotation {1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 scale {1.0f, 1.0f, 1.0f};

        glm::vec3 halfExtents {0.5f, 0.5f, 0.5f};
        float     radius = 0.5f;
        float     height = 1.0f; // Capsule cylinder height (excluding hemispheres)
        /// Local-space offset of the collider relative to body position/rotation.
        glm::vec3 centerOffset {0.0f, 0.0f, 0.0f};

        float mass        = 1.0f;
        float friction    = 0.5f;
        float restitution = 0.0f;

        std::uint8_t  collisionLayer = 0; // 0..15
        std::uint16_t collideWithLayers = 0xffff;

        /// Sensor/trigger volume: generates trigger events, no collision response.
        bool isSensor = false;
        /// Optional ECS entity id stored as body user data (0 = unset).
        std::uint64_t entityId = 0;

        std::vector<glm::vec3> meshPoints; // Used when shape == Mesh
    };

    /// Capsule dimensions for create / runtime resize (crouch).
    struct PhysicsCharacterShapeDesc
    {
        float radius = 0.5f;
        float height = 1.0f; // Capsule cylinder height (excluding hemispheres)
        /// Local offset added on top of automatic feet lift (0, height/2+radius, 0).
        glm::vec3 centerOffset {0.0f, 0.0f, 0.0f};
    };

    struct PhysicsCharacterDesc
    {
        glm::vec3 position {0.0f};
        glm::quat rotation {1.0f, 0.0f, 0.0f, 0.0f};

        float radius          = 0.5f;
        float height          = 1.0f; // Capsule cylinder height (excluding hemispheres)
        float maxSlopeDegrees = 45.0f;
        float mass            = 70.0f;
        /// Max force (N) used when the character pushes dynamic bodies.
        float maxStrength = 100.0f;

        /// Local offset added on top of automatic feet lift (0, height/2+radius, 0).
        glm::vec3 centerOffset {0.0f, 0.0f, 0.0f};

        /// ExtendedUpdate: project down onto floor within this distance. 0 disables stick-to-floor.
        float stickToFloorDistance = 0.5f;
        /// ExtendedUpdate: max step-up height for stairs. 0 disables walk-stairs.
        float walkStairsStepHeight = 0.4f;

        float predictiveContactDistance = 0.1f;
        float characterPadding          = 0.02f;
        float penetrationRecoverySpeed  = 1.0f;
        bool  enhancedInternalEdgeRemoval = false;

        std::uint8_t  collisionLayer    = 1;
        std::uint16_t collideWithLayers = 0xffff;
    };

    /// Support / floor query for a character controller after the last physics step.
    struct CharacterGroundInfo
    {
        CharacterGroundState state = CharacterGroundState::InAir;
        bool                 grounded = false;
        glm::vec3            position {0.0f};
        glm::vec3            normal {0.0f, 1.0f, 0.0f};
        glm::vec3            velocity {0.0f};
        PhysicsBodyHandle    groundBody {};
    };

    struct QueryFilter
    {
        std::uint16_t collideWithLayers = 0xffff;
        /// When set, skip this ECS entity (body user data or bound character).
        std::uint64_t ignoreEntity = 0;
    };

    struct RaycastHit
    {
        bool              hit = false;
        glm::vec3         point {0.0f};
        glm::vec3         normal {0.0f, 1.0f, 0.0f};
        float             fraction = 0.0f;
        float             distance = 0.0f;
        PhysicsBodyHandle body {};
        std::uint64_t     entityId = 0;
    };

    struct OverlapHit
    {
        PhysicsBodyHandle body {};
        std::uint64_t     entityId = 0;
    };

    struct TriggerEvent
    {
        TriggerEventType type = TriggerEventType::Enter;
        std::uint64_t    sensorEntity = 0;
        std::uint64_t    otherEntity  = 0;
    };

    struct PhysicsContactEvent
    {
        PhysicsContactKind kind = PhysicsContactKind::BodyBody;
        std::uint64_t      entityA = 0;
        std::uint64_t      entityB = 0;
        glm::vec3          point {0.0f};
        glm::vec3          normal {0.0f, 1.0f, 0.0f};
        float              penetration = 0.0f;
    };

    struct PhysicsJointDesc
    {
        PhysicsJointType type = PhysicsJointType::Fixed;
        PhysicsBodyHandle bodyA {};
        PhysicsBodyHandle bodyB {};
        glm::vec3 localAnchorA {0.0f};
        glm::vec3 localAnchorB {0.0f};
        /// Hinge axis in body A local space.
        glm::vec3 hingeAxisLocalA {0.0f, 1.0f, 0.0f};
    };

} // namespace FRIGGA_NAMESPACE
