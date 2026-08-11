#pragma once

#include "Frigga/Macro.hpp"
#include "Frigga/Physics/PhysicsCharacterHandle.hpp"

#include <Freyr/Freyr.hpp>

#include <algorithm>
#include <cstdint>
#include <glm/glm.hpp>

namespace FRIGGA_NAMESPACE
{

    /// Capsule character motor driven through fg::Physics (not a RigidBody).
    struct CharacterControllerComponent: fr::Component
    {
        float radius          = 0.35f;
        float height          = 1.0f; // cylinder height excluding hemispheres
        float maxSlopeDegrees = 45.0f;
        float mass            = 70.0f;

        /// Local-space offset of the capsule center from Transform.position, added on top of
        /// the automatic feet lift (0, height/2 + radius, 0).
        glm::vec3 centerOffset {0.0f, 0.0f, 0.0f};

        std::uint8_t  collisionLayer    = 1;
        std::uint16_t collideWithLayers = 0xffff;

        /// Runtime character id — not serialized.
        PhysicsCharacterHandle character {};

        /// Capsule center in transform-local space (feet-lift + centerOffset).
        [[nodiscard]] glm::vec3 CapsuleCenterLocal() const
        {
            const float r      = std::max(radius, 0.001f);
            const float halfH  = std::max(0.5f * height, 0.001f);
            return centerOffset + glm::vec3 {0.0f, halfH + r, 0.0f};
        }
    };

} // namespace FRIGGA_NAMESPACE
