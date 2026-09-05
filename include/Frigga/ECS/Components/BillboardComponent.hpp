#pragma once

#include "Frigga/Macro.hpp"

#include <Freya/Core/BillboardDraw.hpp>
#include <Freyr/Freyr.hpp>

#include <cstdint>
#include <optional>

namespace FRIGGA_NAMESPACE
{

    /// Camera-facing quad submitted to Freya's BillboardDraw each frame.
    struct BillboardComponent: fr::Component
    {
        glm::vec2 size {1.0f, 1.0f};
        glm::vec4 color {1.0f, 1.0f, 1.0f, 1.0f};
        glm::vec4 uvRect {0.0f, 0.0f, 1.0f, 1.0f};
        std::optional<std::uint32_t> textureId;
        fra::BillboardAlign align = fra::BillboardAlign::Screen;
        fra::BillboardBlend blend = fra::BillboardBlend::Alpha;
        fra::BillboardLayer layer = fra::BillboardLayer::Vfx;
        bool      depthTest       = true;
        bool      sdf             = false;
        float     clipMax         = 1.0f;
        glm::vec2 localOffset {0.0f, 0.0f};
    };

} // namespace FRIGGA_NAMESPACE
