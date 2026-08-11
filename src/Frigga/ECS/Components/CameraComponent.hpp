#pragma once

#include <Frigga/Macro.hpp>

#include <Freyr/Freyr.hpp>

namespace FRIGGA_NAMESPACE
{

    struct CameraComponent: fr::Component
    {
        float fovDegrees = 60.0f;
        float nearPlane  = 0.1f;
        float farPlane   = 1000.0f;
        bool  primary    = false;
        bool  locked     = false;
    };

} // namespace FRIGGA_NAMESPACE
