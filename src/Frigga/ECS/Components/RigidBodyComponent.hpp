#pragma once

#include "Frigga/Macro.hpp"
#include "Frigga/Physics/PhysicsBodyHandle.hpp"
#include "Frigga/Physics/PhysicsTypes.hpp"

#include <Freyr/Freyr.hpp>

namespace FRIGGA_NAMESPACE
{

    struct RigidBodyComponent: fr::Component
    {
        BodyMotionType motion = BodyMotionType::Dynamic;
        ColliderShape  shape  = ColliderShape::Box;

        glm::vec3 halfExtents {0.5f, 0.5f, 0.5f};
        float     radius = 0.5f;
        float     height = 1.0f;

        float mass        = 1.0f;
        float friction    = 0.5f;
        float restitution = 0.0f;

        /// Collision layer index [0..15]. Prefer 0 for static, 1+ for dynamic.
        std::uint8_t collisionLayer = 1;
        /// Bitmask of layers this body collides with.
        std::uint16_t collideWithLayers = 0xffff;

        /// Runtime Jolt body id — not serialized.
        PhysicsBodyHandle body {};
    };

} // namespace FRIGGA_NAMESPACE
