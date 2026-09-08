#pragma once

#include <Freyr/Freyr.hpp>

struct CharacterControllerComponent: fr::Component
{
    float maxSlopeDegrees = 45.0f;
    /// Max push force against dynamic bodies (Newtons).
    float maxStrength = 100.0f;
    /// ExtendedUpdate stick-to-floor distance; 0 disables.
    float stickToFloorDistance = 0.5f;
    /// ExtendedUpdate max stair step height; 0 disables.
    float walkStairsStepHeight = 0.4f;
    /// When true, locomotion systems skip WASD drive.
    bool locomotionLocked = false;
};
