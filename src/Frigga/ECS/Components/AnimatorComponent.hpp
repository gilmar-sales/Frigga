#pragma once

#include "Frigga/Animation/AnimGraphDefinition.hpp"

#include <Frigga/Macro.hpp>

#include <Freya/Asset/InstanceTransform.hpp>
#include <Freyr/Freyr.hpp>

#include <cstdint>
#include <string>

namespace FRIGGA_NAMESPACE
{

    /**
     * @brief Drives skinned mesh playback from a ModelAsset loaded via
     *        AssetRegistry (skeleton + clips from Freya CreateSkinnedModelFromFile).
     *
     * Runtime fields `boneOffset` / `boneCount` are written each frame by
     * AnimationSystem for RenderSystem instance uploads.
     */
    struct AnimatorComponent: fr::Component
    {
        /// Resource path under Resources/ (e.g. "Models/Fox.glb").
        std::string modelSource;
        /// Empty selects the first clip on the model.
        std::string clipName;

        float timeSec = 0.0f;
        float speed   = 1.0f;
        bool  playing = true;
        bool  loop    = true;
        /// Prefer GpuAnimPass when a single skeleton is actively GPU-skinned.
        bool useGpu = false;
        /// Advance / sample pose while the editor is in Edit mode.
        bool previewInEdit = true;
        /// Drive playback from `animGraph` instead of a single `clipName`.
        bool useAnimGraph = false;
        AnimGraphDefinition animGraph {};

        std::uint32_t boneOffset = fra::kNoSkin;
        std::uint32_t boneCount  = 0;
    };

} // namespace FRIGGA_NAMESPACE
