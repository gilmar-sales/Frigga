#pragma once

#include "Frigga/ECS/Components/TransformComponent.hpp"

namespace FRIGGA_NAMESPACE
{

    struct EditorCamera
    {
        TransformComponent transform {};
        float              fovDegrees = 50.0f;
        float              nearPlane  = 0.1f;
        float              farPlane   = 1000.0f;
    };

} // namespace FRIGGA_NAMESPACE
