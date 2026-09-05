#pragma once

#include "Frigga/ECS/Components/FullscreenEffectComponent.hpp"

#include <Freya/Builders/PostProcessBuilder.hpp>
#include <Freya/Core/PostProcess.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace FRIGGA_NAMESPACE
{

    struct FullscreenEffectPushState
    {
        float timeSec     = 0.0f;
        bool  reverseZ    = false;
        const FullscreenEffectComponent *component = nullptr;
    };

    [[nodiscard]] std::string FullscreenEffectFragmentPath(FullscreenEffectKind kind,
                                                           std::string_view customFragment);

    void ConfigureFullscreenEffectBuilder(fra::PostProcessBuilder &builder,
                                          std::string_view stageName,
                                          const FullscreenEffectComponent &component);

    void ApplyFullscreenEffectPushConstants(fra::PostProcess &effect,
                                            const FullscreenEffectPushState &state);

    void SyncFullscreenEffectMaterials(fra::PostProcess &effect,
                                       const FullscreenEffectComponent &component);

} // namespace FRIGGA_NAMESPACE
