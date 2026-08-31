#pragma once

#include "Frigga/Macro.hpp"

#include <Freyr/Freyr.hpp>

#include <glm/glm.hpp>

namespace FRIGGA_NAMESPACE
{

    /// Cylindrical nameplate (background + fill) via Freya BillboardDraw::HealthBar.
    struct HealthBarComponent: fr::Component
    {
        float     fill   = 1.0f;
        float     width  = 0.85f;
        float     height = 0.08f;
        glm::vec3 offset {0.0f, 1.25f, 0.0f};
        glm::vec4 background {0.08f, 0.08f, 0.08f, 0.85f};
        glm::vec4 foreground {0.25f, 0.85f, 0.35f, 1.0f};
    };

} // namespace FRIGGA_NAMESPACE
