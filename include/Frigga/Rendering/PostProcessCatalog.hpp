#pragma once

#include "Frigga/ECS/Components/PostProcessComponent.hpp"

#include <Freya/Advanced.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace FRIGGA_NAMESPACE
{

    struct PostProcessPushState
    {
        float timeSec     = 0.0f;
        bool  reverseZ    = false;
        const PostProcessComponent *component = nullptr;
    };

    [[nodiscard]] std::string PostProcessFragmentPath(PostProcessKind kind,
                                                      std::string_view customFragment);

    void ConfigurePostProcessBuilder(fra::PostProcessBuilder &builder,
                                     std::string_view stageName,
                                     const PostProcessComponent &component);

    void ApplyPostProcessPushConstants(fra::PostProcess &effect,
                                       const PostProcessPushState &state);

    void SyncPostProcessMaterials(fra::PostProcess &effect,
                                  const PostProcessComponent &component);

} // namespace FRIGGA_NAMESPACE
