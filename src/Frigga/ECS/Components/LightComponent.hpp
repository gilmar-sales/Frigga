#pragma once

#include <Freya/Core/UniformBuffer.hpp>
#include <Freyr/Freyr.hpp>

namespace FRIGGA_NAMESPACE
{

    struct LightComponent: fr::Component
    {
        fra::LightType type      = fra::LightType::Point;
        glm::vec3      color     = {1.0f, 1.0f, 1.0f};
        float          radius    = 40.0f;
        float          intensity = 30.0f;
        /// Spot cone half-angles in degrees (converted to cosines for Freya).
        float innerAngleDegrees = 25.0f;
        float outerAngleDegrees = 35.0f;
        /// Area light rectangle half-extents (Freya LTC).
        float halfWidth  = 1.0f;
        float halfHeight = 1.0f;
    };

} // namespace FRIGGA_NAMESPACE
