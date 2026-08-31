#pragma once

#include "Frigga/Macro.hpp"

#include <Freyr/Freyr.hpp>

#include <glm/glm.hpp>

#include <string>

namespace FRIGGA_NAMESPACE
{

    /// Hosts a Freya FullscreenEffect (custom SPIR-V, e.g. Cell/cell.frag.spv).
    struct FullscreenEffectComponent: fr::Component
    {
        std::string name     = "Cell";
        std::string fragment = "Cell/cell.frag.spv";
        bool        enabled  = true;

        float     bands           = 4.0f;
        float     edgeDepthScale  = 80.0f;
        float     edgeNormalScale = 2.0f;
        float     strength        = 1.0f;
        glm::vec4 edgeColor {0.02f, 0.02f, 0.04f, 1.0f};
        float     shadowLift = 0.22f;
        float     edgeWidth  = 1.0f;
    };

} // namespace FRIGGA_NAMESPACE
