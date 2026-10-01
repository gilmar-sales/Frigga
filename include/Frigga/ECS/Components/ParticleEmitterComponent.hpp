#pragma once

#include "Frigga/Macro.hpp"

#include <Freya/Core/ParticleEmitter.hpp>

#include <cstdint>
#include <optional>

namespace FRIGGA_NAMESPACE
{

    struct ParticleEmitterComponent
    {
        bool playing = true;
        std::optional<std::uint32_t> textureId;
        fra::ParticleEmitter runtime = {};
    };

} // namespace FRIGGA_NAMESPACE
