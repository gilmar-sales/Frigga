#include <Frigga/Animation/AnimGraphDefinition.hpp>

#include <algorithm>
#include <format>
#include <stdexcept>

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        const fra::AnimationClip *FindClip(const ModelAsset &model, std::string_view name)
        {
            if(name.empty())
            {
                return model.clips.empty() ? nullptr : &model.clips.front();
            }
            for(const auto &clip : model.clips)
            {
                if(clip.name == name)
                {
                    return &clip;
                }
            }
            return nullptr;
        }

        fra::AnimCondition MakeCondition(const AnimGraphTransitionDef &transition)
        {
            if(transition.conditionKind == "FloatGreater")
            {
                return fra::AnimCondition::FloatGreater(transition.param, transition.threshold);
            }
            if(transition.conditionKind == "FloatLessEqual")
            {
                return fra::AnimCondition::FloatLessEqual(transition.param, transition.threshold);
            }
            if(transition.conditionKind == "BoolTrue")
            {
                return fra::AnimCondition::BoolTrue(transition.param);
            }
            if(transition.conditionKind == "BoolFalse")
            {
                return fra::AnimCondition::BoolFalse(transition.param);
            }
            return fra::AnimCondition::OnTrigger(transition.param);
        }
    } // namespace

    std::string AnimGraphFingerprint(const AnimGraphDefinition &graph,
                                     std::string_view modelSource)
    {
        std::string out;
        out += modelSource;
        out += '|';
        out += graph.entry;
        for(const auto &param : graph.params)
        {
            out += param.name;
            out += param.kind;
        }
        for(const auto &state : graph.states)
        {
            out += state.name;
            out += state.kind;
            out += state.clip;
            out += state.blendParam;
            out += state.blendParamY;
            out += state.loop ? '1' : '0';
            out += std::to_string(state.playbackSpeed);
            for(const auto &sample : state.blendSamples)
            {
                out += sample.clip;
                out += std::to_string(sample.value);
                out += std::to_string(sample.x);
                out += std::to_string(sample.y);
            }
        }
        for(const auto &transition : graph.transitions)
        {
            out += transition.from;
            out += transition.to;
            out += transition.conditionKind;
            out += transition.param;
            out += std::to_string(transition.threshold);
            out += std::to_string(transition.blendDuration);
        }
        return out;
    }

    std::optional<fra::AnimGraph> CompileAnimGraph(const AnimGraphDefinition &graph,
                                                   const ModelAsset &model)
    {
        if(!model.skinned || model.skeleton.JointCount() == 0 || graph.states.empty())
        {
            return std::nullopt;
        }

        try
        {
            fra::AnimGraphBuilder builder;
            builder.SetSkeleton(&model.skeleton);

            for(const auto &param : graph.params)
            {
                if(param.name.empty())
                {
                    continue;
                }
                if(param.kind == "Bool")
                {
                    builder.ParamBool(param.name, param.defaultBool);
                }
                else if(param.kind == "Trigger")
                {
                    builder.ParamTrigger(param.name);
                }
                else if(param.hasRange)
                {
                    builder.ParamFloat(param.name, param.defaultFloat, param.minValue,
                                       param.maxValue);
                }
                else
                {
                    builder.ParamFloat(param.name, param.defaultFloat);
                }
            }

            for(const auto &state : graph.states)
            {
                if(state.name.empty())
                {
                    continue;
                }
                if(state.kind == "Blend1D")
                {
                    builder.Blend1DState(state.name, state.blendParam, state.syncPhase);
                    for(const auto &sample : state.blendSamples)
                    {
                        const auto *clip = FindClip(model, sample.clip);
                        if(clip == nullptr)
                        {
                            continue;
                        }
                        builder.AddBlendSample(sample.value, *clip, sample.loop,
                                               sample.playbackSpeed);
                    }
                }
                else if(state.kind == "Blend2D")
                {
                    builder.Blend2DState(state.name, state.blendParam, state.blendParamY,
                                         state.syncPhase);
                    for(const auto &sample : state.blendSamples)
                    {
                        const auto *clip = FindClip(model, sample.clip);
                        if(clip == nullptr)
                        {
                            continue;
                        }
                        builder.AddBlend2DSample(sample.x, sample.y, *clip, sample.loop,
                                                 sample.playbackSpeed);
                    }
                }
                else
                {
                    const auto *clip = FindClip(model, state.clip);
                    if(clip == nullptr)
                    {
                        return std::nullopt;
                    }
                    builder.State(state.name, *clip, state.loop, state.playbackSpeed);
                }
            }

            if(!graph.entry.empty())
            {
                builder.Entry(graph.entry);
            }

            for(const auto &transition : graph.transitions)
            {
                if(transition.from.empty() || transition.to.empty())
                {
                    continue;
                }
                builder.Transition(transition.from, transition.to, MakeCondition(transition),
                                   transition.blendDuration);
            }

            return builder.Build();
        }
        catch(const std::exception &)
        {
            return std::nullopt;
        }
    }

    std::string UniqueAnimGraphStateName(const AnimGraphDefinition &graph, std::string_view base)
    {
        std::string name {base.empty() ? "State" : base};
        if(FindAnimGraphStateIndex(graph, name) < 0)
        {
            return name;
        }
        for(int i = 2; i < 1000; ++i)
        {
            auto candidate = std::format("{} {}", name, i);
            if(FindAnimGraphStateIndex(graph, candidate) < 0)
            {
                return candidate;
            }
        }
        return std::format("{}_{}", name, graph.states.size());
    }

    int FindAnimGraphStateIndex(const AnimGraphDefinition &graph, std::string_view name)
    {
        for(int i = 0; i < static_cast<int>(graph.states.size()); ++i)
        {
            if(graph.states[static_cast<std::size_t>(i)].name == name)
            {
                return i;
            }
        }
        return -1;
    }

    void RenameAnimGraphState(AnimGraphDefinition &graph, std::string_view from, std::string to)
    {
        if(from.empty() || to.empty() || from == to)
        {
            return;
        }
        if(FindAnimGraphStateIndex(graph, to) >= 0)
        {
            return;
        }
        for(auto &state : graph.states)
        {
            if(state.name == from)
            {
                state.name = to;
            }
        }
        if(graph.entry == from)
        {
            graph.entry = to;
        }
        for(auto &transition : graph.transitions)
        {
            if(transition.from == from)
            {
                transition.from = to;
            }
            if(transition.to == from)
            {
                transition.to = to;
            }
        }
    }

    void LayoutAnimGraphIfNeeded(AnimGraphDefinition &graph)
    {
        if(graph.states.empty())
        {
            return;
        }
        const bool needsLayout = std::ranges::all_of(
            graph.states, [](const AnimGraphStateDef &state) {
                return state.editorX == 0.0f && state.editorY == 0.0f;
            });
        if(!needsLayout)
        {
            return;
        }

        constexpr float kOriginX = 80.0f;
        constexpr float kOriginY = 80.0f;
        constexpr float kStepX   = 240.0f;
        constexpr float kStepY   = 140.0f;
        constexpr int   kColumns = 3;
        for(std::size_t i = 0; i < graph.states.size(); ++i)
        {
            graph.states[i].editorX =
                kOriginX + static_cast<float>(i % kColumns) * kStepX;
            graph.states[i].editorY =
                kOriginY + static_cast<float>(i / kColumns) * kStepY;
        }
    }

    void PopulateAnimGraphFromClips(AnimGraphDefinition &graph, const ModelAsset &model)
    {
        graph.states.clear();
        graph.transitions.clear();
        graph.states.reserve(model.clips.size());
        for(const auto &clip : model.clips)
        {
            AnimGraphStateDef state {};
            state.name = UniqueAnimGraphStateName(graph, clip.name);
            state.kind = "Clip";
            state.clip = clip.name;
            graph.states.push_back(std::move(state));
        }
        graph.entry = graph.states.empty() ? std::string {} : graph.states.front().name;
        for(auto &state : graph.states)
        {
            state.editorX = 0.0f;
            state.editorY = 0.0f;
        }
        LayoutAnimGraphIfNeeded(graph);
    }

    bool HasAnimGraphTransition(const AnimGraphDefinition &graph, std::string_view from,
                                std::string_view to)
    {
        return std::ranges::any_of(graph.transitions, [&](const AnimGraphTransitionDef &tr) {
            return tr.from == from && tr.to == to;
        });
    }

} // namespace FRIGGA_NAMESPACE
