#pragma once

#include "Frigga/Macro.hpp"
#include "Frigga/Physics/PhysicsCharacterHandle.hpp"

#include <Freyr/Freyr.hpp>

#include <cstdint>

namespace FRIGGA_NAMESPACE
{

    /// Capsule character motor driven through fg::Physics (not a RigidBody).
    struct CharacterControllerComponent: fr::Component
    {
        float radius          = 0.35f;
        float height          = 1.0f; // cylinder height excluding hemispheres
        float maxSlopeDegrees = 45.0f;
        float mass            = 70.0f;

        std::uint8_t  collisionLayer    = 1;
        std::uint16_t collideWithLayers = 0xffff;

        /// Runtime character id — not serialized.
        PhysicsCharacterHandle character {};
    };

} // namespace FRIGGA_NAMESPACE
