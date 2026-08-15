#pragma once

#include <Freyr/Freyr.hpp>
#include <glm/glm.hpp>

#include <string>

struct ThirdPersonCameraComponent: fr::Component
{
    std::string targetName = "Player";
    glm::vec3   pivotOffset {0.0f, 1.4f, 0.0f};

    float distance    = 6.0f;
    float minDistance = 1.5f;
    float maxDistance = 14.0f;

    float yaw      = 0.0f;
    float pitch    = 18.0f;
    float minPitch = -35.0f;
    float maxPitch = 70.0f;

    std::string lookXAxis = "LookX";
    std::string lookYAxis = "LookY";
    std::string zoomAxis  = "Zoom";
};
