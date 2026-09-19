#pragma once

#include "Frigga/Animation/AnimGraphDefinition.hpp"

#include <Frigga/Macro.hpp>

#include <Freya/Asset/InstanceTransform.hpp>
#include <Freyr/Freyr.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace FRIGGA_NAMESPACE
{

    /**
     * @brief Drives skinned mesh playback from a ModelAsset loaded via
     *        AssetRegistry (skeleton + clips from Freya CreateSkinnedModelFromFile).
     *
     * Prefer one Animator on a hierarchy root (Unity-like). Child Mesh entities
     * without their own Animator inherit that root's bone palette via parent walk
     * in RenderSystem. Same-entity Mesh+Animator remains supported for single meshes.
     *
     * Runtime fields `boneOffset` / `boneCount` are allocated once per model
     * (stable palette) by AnimationSystem for RenderSystem instance uploads.
     * LOD / clipTimePrev live on the component so EachAsync never touches
     * shared system maps.
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

        /// When set, Footstep.* clip markers route to this audio event path.
        std::string footstepEventPath;
        /// Optional explicit marker name → audio event path overrides.
        std::vector<std::pair<std::string, std::string>> clipEventRoutes;
        /// When false, clip markers are ignored by AnimationEventRouter.
        bool routeClipEvents = true;

        /// Model path the current boneOffset was allocated for (runtime).
        std::string bonePaletteSource;
        std::uint32_t boneOffset = fra::kNoSkin;
        std::uint32_t boneCount  = 0;

        /// Per-entity anim LOD (runtime; not serialized).
        float        lodAccum = 0.f;
        std::uint8_t lodTier  = 0;
        /// Previous clip time for event edge detection (runtime).
        float clipTimePrev = 0.f;
    };

} // namespace FRIGGA_NAMESPACE
