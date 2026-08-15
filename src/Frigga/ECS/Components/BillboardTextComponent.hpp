#pragma once

#include "Frigga/Macro.hpp"

#include <Freya/Core/BillboardDraw.hpp>
#include <Freyr/Freyr.hpp>

#include <string>

namespace FRIGGA_NAMESPACE
{

    /// World-space SDF nameplate (Freya BillboardDraw::Text). Needs a loaded FontAtlas.
    struct BillboardTextComponent: fr::Component
    {
        std::string text         = "Label";
        std::string fontSource   = "Fonts/NotoSans-Regular.ttf";
        float       heightMeters = 0.16f;
        glm::vec4   color {0.95f, 0.98f, 0.92f, 1.0f};
        float       borderWidth  = 0.0f;
        glm::vec4   borderColor {0.0f, 0.0f, 0.0f, 1.0f};
        glm::vec3   offset {0.0f, 0.4f, 0.0f};
        fra::BillboardAlign align = fra::BillboardAlign::Cylindrical;
        fra::BillboardLayer layer = fra::BillboardLayer::Ui;
    };

} // namespace FRIGGA_NAMESPACE
