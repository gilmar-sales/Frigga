#pragma once

#include "Frigga/Macro.hpp"

#include <Freya/Core/ParticleEmitter.hpp>
#include <Freyr/Freyr.hpp>

#include <cstdint>
#include <optional>

namespace FRIGGA_NAMESPACE
{

    struct ParticleEmitterComponent: fr::Component
    {
        bool playing = true;
        std::optional<std::uint32_t> textureId;
        fra::ParticleEmitter runtime = {};

        ParticleEmitterComponent() = default;

        ParticleEmitterComponent(const ParticleEmitterComponent &other)
            : playing(other.playing), textureId(other.textureId), runtime({})
        {
            copyParams(other.runtime, runtime);
        }

        ParticleEmitterComponent &operator=(const ParticleEmitterComponent &other)
        {
            if(this == &other)
            {
                return *this;
            }
            playing   = other.playing;
            textureId = other.textureId;
            runtime   = {};
            copyParams(other.runtime, runtime);
            return *this;
        }

        ParticleEmitterComponent(ParticleEmitterComponent &&) noexcept            = default;
        ParticleEmitterComponent &operator=(ParticleEmitterComponent &&) noexcept = default;

      private:
        static void copyParams(const fra::ParticleEmitter &from, fra::ParticleEmitter &to)
        {
            to.velocity       = from.velocity;
            to.velocityJitter = from.velocityJitter;
            to.spawnRate      = from.spawnRate;
            to.lifetime       = from.lifetime;
            to.size0          = from.size0;
            to.size1          = from.size1;
            to.color0         = from.color0;
            to.color1         = from.color1;
            to.blend          = from.blend;
            to.maxParticles   = from.maxParticles;
        }
    };

} // namespace FRIGGA_NAMESPACE
