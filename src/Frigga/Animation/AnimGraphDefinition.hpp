#pragma once

#include "Frigga/Asset/AssetRegistry.hpp"

#include <Freya/Asset/AnimGraph.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace FRIGGA_NAMESPACE
{

    struct AnimGraphParamDef
    {
        std::string name;
        std::string kind {"Float"};
        float       defaultFloat = 0.0f;
        bool        defaultBool  = false;
        float       minValue     = 0.0f;
        float       maxValue     = 1.0f;
        bool        hasRange     = false;
    };

    struct AnimGraphBlendSampleDef
    {
        float       value = 0.0f;
        float       x     = 0.0f;
        float       y     = 0.0f;
        std::string clip;
        bool        loop          = true;
        float       playbackSpeed = 1.0f;
    };

    struct AnimGraphStateDef
    {
        std::string name;
        std::string kind {"Clip"};
        std::string clip;
        bool        loop          = true;
        float       playbackSpeed = 1.0f;
        std::string blendParam;
        std::string blendParamY;
        bool        syncPhase = true;
        std::vector<AnimGraphBlendSampleDef> blendSamples;
        float editorX = 0.0f;
        float editorY = 0.0f;
    };

    struct AnimGraphTransitionDef
    {
        std::string from;
        std::string to;
        std::string conditionKind {"Trigger"};
        std::string param;
        float       threshold     = 0.0f;
        float       blendDuration = 0.2f;
    };

    struct AnimGraphDefinition
    {
        std::string                          entry;
        std::vector<AnimGraphParamDef>       params;
        std::vector<AnimGraphStateDef>       states;
        std::vector<AnimGraphTransitionDef>  transitions;
    };

    [[nodiscard]] std::string AnimGraphFingerprint(const AnimGraphDefinition &graph,
                                                   std::string_view modelSource);

    [[nodiscard]] std::optional<fra::AnimGraph>
    CompileAnimGraph(const AnimGraphDefinition &graph, const ModelAsset &model);

    [[nodiscard]] std::string UniqueAnimGraphStateName(const AnimGraphDefinition &graph,
                                                       std::string_view base);

    [[nodiscard]] int FindAnimGraphStateIndex(const AnimGraphDefinition &graph,
                                              std::string_view name);

    void RenameAnimGraphState(AnimGraphDefinition &graph, std::string_view from,
                              std::string to);

    void LayoutAnimGraphIfNeeded(AnimGraphDefinition &graph);

    void PopulateAnimGraphFromClips(AnimGraphDefinition &graph, const ModelAsset &model);

    [[nodiscard]] bool HasAnimGraphTransition(const AnimGraphDefinition &graph,
                                              std::string_view from, std::string_view to);

} // namespace FRIGGA_NAMESPACE
