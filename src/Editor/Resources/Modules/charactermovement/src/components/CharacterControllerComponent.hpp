#pragma once

#include <Freyr/Freyr.hpp>

struct CharacterControllerComponent: fr::Component
{
    float maxSlopeDegrees = 45.0f;
    /// When true, locomotion systems skip WASD / AI drive.
    bool locomotionLocked = false;
};
