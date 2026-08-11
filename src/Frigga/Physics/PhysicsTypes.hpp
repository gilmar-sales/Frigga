#pragma once

#include "Frigga/Macro.hpp"

#include <cstdint>
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

        float mass        = 1.0f;
        float friction    = 0.5f;
        float restitution = 0.0f;

        std::uint8_t  collisionLayer = 0; // 0..15
        std::uint16_t collideWithLayers = 0xffff;

        std::vector<glm::vec3> meshPoints; // Used when shape == Mesh
    };

    struct PhysicsCharacterDesc
    {
        glm::vec3 position {0.0f};
        glm::quat rotation {1.0f, 0.0f, 0.0f, 0.0f};

        float radius          = 0.35f;
        float height          = 1.0f; // Capsule cylinder height (excluding hemispheres)
        float maxSlopeDegrees = 45.0f;
        float mass            = 70.0f;

        /// Local offset added on top of automatic feet lift (0, height/2+radius, 0).
        glm::vec3 centerOffset {0.0f, 0.0f, 0.0f};

        std::uint8_t  collisionLayer    = 1;
        std::uint16_t collideWithLayers = 0xffff;
    };

} // namespace FRIGGA_NAMESPACE
