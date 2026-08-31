#pragma once

#include "Frigga/Macro.hpp"

#include <Freyr/Freyr.hpp>

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace FRIGGA_NAMESPACE
{

    enum class FullscreenEffectKind : std::uint8_t
    {
        Cell = 0,
        Outline,
        ColorGrade,
        Underwater,
        HeatHaze,
        Glow,
        MuItemGlow,
        Custom,
    };

    /// Hosts a Freya PostProcess fullscreen pass (stock or custom SPIR-V).
    struct FullscreenEffectComponent: fr::Component
    {
        FullscreenEffectKind kind     = FullscreenEffectKind::Cell;
        std::string          name     = "Cell";
        std::string          fragment = "Cell/cell.frag.spv";
        bool                 enabled  = true;

        /// G-buffer material IDs (0–255) masked by BindMaterial. Empty = all pixels.
        std::vector<std::uint32_t> materialMaskIds;

        float     bands           = 4.0f;
        float     edgeDepthScale  = 80.0f;
        float     edgeNormalScale = 2.0f;
        float     strength        = 1.0f;
        glm::vec4 edgeColor {0.02f, 0.02f, 0.04f, 1.0f};
        float     shadowLift = 0.22f;
        float     edgeWidth  = 1.0f;

        float     contrast   = 1.05f;
        float     saturation = 1.15f;
        float     exposure   = 0.0f;
        float     vignette   = 0.35f;
        glm::vec4 lift {0.0f};
        glm::vec4 gain {1.0f, 1.0f, 1.0f, 1.0f};

        float     tintStrength = 0.55f;
        float     fogDensity   = 1.8f;
        glm::vec4 tintColor {0.15f, 0.45f, 0.55f, 1.0f};
        float     maxDepth     = 0.85f;

        float     heatSpeed = 1.2f;

        float     glowIntensity = 2.2f;
        float     glowRadius    = 8.0f;
        float     glowFill      = 0.25f;
        glm::vec4 glowColor {1.0f, 0.85f, 0.25f, 1.0f};
        float     muGlowLevel   = 13.0f;
    };

} // namespace FRIGGA_NAMESPACE
