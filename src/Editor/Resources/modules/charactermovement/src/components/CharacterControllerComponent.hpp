#pragma once

#include <Freyr/Freyr.hpp>
#include <glm/glm.hpp>

#include <cstdint>

struct CharacterControllerComponent: fr::Component
{
    float radius          = 0.35f;
    float height          = 1.0f;
    float maxSlopeDegrees = 45.0f;
    float mass            = 70.0f;
    glm::vec3 centerOffset {0.0f, 0.0f, 0.0f};
    std::uint8_t  collisionLayer    = 1;
    std::uint16_t collideWithLayers = 0xffff;
};
