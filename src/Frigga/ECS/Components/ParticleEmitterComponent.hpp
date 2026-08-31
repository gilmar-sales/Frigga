#pragma once

#include "Frigga/Macro.hpp"

#include <Freya/Core/BillboardDraw.hpp>
#include <Freyr/Freyr.hpp>

#include <cstdint>
#include <optional>

namespace FRIGGA_NAMESPACE
{

    /// CPU particle emitter (Freya ParticleEmitter) that pushes VFX billboards.
    struct ParticleEmitterComponent: fr::Component
    {
        glm::vec3 velocity {0.0f, 1.2f, 0.0f};
        glm::vec3 velocityJitter {0.35f, 0.25f, 0.35f};
        float     spawnRate = 24.0f;
        float     lifetime  = 0.7f;
        float     size0     = 0.12f;
        float     size1     = 0.02f;
        glm::vec4 color0 {0.35f, 0.85f, 1.0f, 1.0f};
        glm::vec4 color1 {0.1f, 0.2f, 1.0f, 0.0f};
        fra::BillboardBlend blend = fra::BillboardBlend::Additive;
        std::optional<std::uint32_t> textureId;
        std::uint32_t maxParticles = 256;
        bool          playing      = true;
    };

} // namespace FRIGGA_NAMESPACE
