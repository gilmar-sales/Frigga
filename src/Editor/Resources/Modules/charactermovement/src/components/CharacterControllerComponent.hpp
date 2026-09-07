#pragma once

#include <Freyr/Freyr.hpp>
#include <glm/glm.hpp>

#include <cstdint>

struct CharacterControllerComponent: fr::Component
{
    float radius          = 0.5f;
    float height          = 1.0f;
    float maxSlopeDegrees = 45.0f;
    float mass            = 70.0f;
    /// Max push force against dynamic bodies (Newtons).
    float maxStrength = 100.0f;
    glm::vec3 centerOffset {0.0f, 0.0f, 0.0f};
    /// ExtendedUpdate stick-to-floor distance; 0 disables.
    float stickToFloorDistance = 0.5f;
    /// ExtendedUpdate max stair step height; 0 disables.
    float walkStairsStepHeight = 0.4f;

    float predictiveContactDistance = 0.1f;
    float characterPadding          = 0.02f;
    float penetrationRecoverySpeed  = 1.0f;
    bool  enhancedInternalEdgeRemoval = false;

    std::uint8_t  collisionLayer    = 1;
    std::uint16_t collideWithLayers = 0xffff;
};
