#include "AnimationController.hpp"

#include "Frigga/Animation/AnimGraphDefinition.hpp"

#include <algorithm>

namespace FRIGGA_NAMESPACE
{
    AnimationController::AnimationController(const skr::Arc<fr::Registry> &registry,
                                             const skr::Arc<AssetRegistry> &assets)
        : mRegistry(registry), mAssets(assets)
    {
    }

    AnimationController::EntityRuntime &AnimationController::EnsureRuntime(fr::Entity entity)
    {
        return mRuntimes[entity];
    }

    AnimationController::EntityRuntime *AnimationController::TryGetRuntime(fr::Entity entity)
    {
        const auto it = mRuntimes.find(entity);
        return it == mRuntimes.end() ? nullptr : &it->second;
    }

    const AnimationController::EntityRuntime *AnimationController::TryGetRuntime(
        fr::Entity entity) const
    {
        const auto it = mRuntimes.find(entity);
        return it == mRuntimes.end() ? nullptr : &it->second;
    }

    void AnimationController::ClearRuntime(fr::Entity entity)
    {
        mRuntimes.erase(entity);
    }

    bool AnimationController::resolveClipName(fr::Entity entity, std::string_view requested,
                                              std::string &outClipName) const
    {
        if(!mRegistry->HasComponent<AnimatorComponent>(entity))
        {
            return false;
        }

        std::string modelSource;
        mRegistry->TryGetComponents<AnimatorComponent>(
            entity, [&](AnimatorComponent &animator) { modelSource = animator.modelSource; });

        if(modelSource.empty())
        {
            return false;
        }

        const auto *model = mAssets->FindModel(modelSource);
        if(model == nullptr)
        {
            (void)mAssets->LoadModel(modelSource);
            model = mAssets->FindModel(modelSource);
        }
        if(model == nullptr || model->clips.empty())
        {
            return false;
        }

        if(requested.empty())
        {
            outClipName = model->clips.front().name;
            return true;
        }

        for(const auto &clip : model->clips)
        {
            if(clip.name == requested)
            {
                outClipName = clip.name;
                return true;
            }
        }
        for(const auto &clip : model->clips)
        {
            if(clip.name.find(requested) != std::string::npos)
            {
                outClipName = clip.name;
                return true;
            }
        }
        return false;
    }

    bool AnimationController::Play(fr::Entity entity, std::string_view clipName,
                                   float crossFadeSeconds)
    {
        if(!mRegistry->HasComponent<AnimatorComponent>(entity))
        {
            return false;
        }

        std::string resolved;
        if(!resolveClipName(entity, clipName, resolved))
        {
            return false;
        }

        bool ok = false;
        mRegistry->TryGetComponents<AnimatorComponent>(entity, [&](AnimatorComponent &animator) {
            auto &runtime = EnsureRuntime(entity);

            const bool sameClip = animator.clipName == resolved;
            if(crossFadeSeconds > 0.0f && !sameClip && !animator.clipName.empty())
            {
                runtime.fromClip          = animator.clipName;
                runtime.fromTimeSec       = animator.timeSec;
                runtime.crossFading       = true;
                runtime.crossFadeDuration = crossFadeSeconds;
                runtime.crossFadeElapsed  = 0.0f;
            }
            else
            {
                runtime.crossFading = false;
            }

            animator.clipName = resolved;
            animator.timeSec  = 0.0f;
            animator.playing  = true;
            ok                = true;
        });
        return ok;
    }

    bool AnimationController::CrossFade(fr::Entity entity, std::string_view clipName,
                                        float durationSeconds)
    {
        return Play(entity, clipName, std::max(durationSeconds, 0.0f));
    }

    void AnimationController::Stop(fr::Entity entity)
    {
        mRegistry->TryGetComponents<AnimatorComponent>(entity, [&](AnimatorComponent &animator) {
            animator.playing = false;
            animator.timeSec = 0.0f;
            if(auto *runtime = TryGetRuntime(entity))
            {
                runtime->crossFading = false;
            }
        });
    }

    void AnimationController::Pause(fr::Entity entity)
    {
        mRegistry->TryGetComponents<AnimatorComponent>(
            entity, [](AnimatorComponent &animator) { animator.playing = false; });
    }

    void AnimationController::Resume(fr::Entity entity)
    {
        mRegistry->TryGetComponents<AnimatorComponent>(
            entity, [](AnimatorComponent &animator) { animator.playing = true; });
    }

    void AnimationController::SetSpeed(fr::Entity entity, float speed)
    {
        mRegistry->TryGetComponents<AnimatorComponent>(
            entity, [speed](AnimatorComponent &animator) { animator.speed = speed; });
    }

    void AnimationController::SetLoop(fr::Entity entity, bool loop)
    {
        mRegistry->TryGetComponents<AnimatorComponent>(
            entity, [loop](AnimatorComponent &animator) { animator.loop = loop; });
    }

    void AnimationController::SetTime(fr::Entity entity, float timeSec)
    {
        mRegistry->TryGetComponents<AnimatorComponent>(
            entity, [timeSec](AnimatorComponent &animator) { animator.timeSec = timeSec; });
    }

    void AnimationController::SetFloat(fr::Entity entity, std::string_view name, float value)
    {
        EnsureRuntime(entity).floats[std::string(name)] = value;
    }

    void AnimationController::SetBool(fr::Entity entity, std::string_view name, bool value)
    {
        EnsureRuntime(entity).bools[std::string(name)] = value;
    }

    void AnimationController::SetTrigger(fr::Entity entity, std::string_view name)
    {
        EnsureRuntime(entity).triggers[std::string(name)] = true;
    }

    float AnimationController::GetFloat(fr::Entity entity, std::string_view name,
                                        float fallback) const
    {
        const auto *runtime = TryGetRuntime(entity);
        if(runtime == nullptr)
        {
            return fallback;
        }
        const auto it = runtime->floats.find(std::string(name));
        return it == runtime->floats.end() ? fallback : it->second;
    }

    bool AnimationController::GetBool(fr::Entity entity, std::string_view name, bool fallback) const
    {
        const auto *runtime = TryGetRuntime(entity);
        if(runtime == nullptr)
        {
            return fallback;
        }
        const auto it = runtime->bools.find(std::string(name));
        return it == runtime->bools.end() ? fallback : it->second;
    }

    bool AnimationController::ConsumeTrigger(fr::Entity entity, std::string_view name)
    {
        auto *runtime = TryGetRuntime(entity);
        if(runtime == nullptr)
        {
            return false;
        }
        const auto it = runtime->triggers.find(std::string(name));
        if(it == runtime->triggers.end() || !it->second)
        {
            return false;
        }
        it->second = false;
        return true;
    }

    std::string_view AnimationController::GetState(fr::Entity entity) const
    {
        if(const auto *runtime = TryGetRuntime(entity);
           runtime != nullptr && runtime->animGraph)
        {
            return runtime->animGraph->CurrentStateName();
        }

        std::string_view state {};
        mRegistry->TryGetComponents<AnimatorComponent>(
            entity, [&](AnimatorComponent &animator) { state = animator.clipName; });
        return state;
    }

    bool AnimationController::IsPlaying(fr::Entity entity) const
    {
        bool playing = false;
        mRegistry->TryGetComponents<AnimatorComponent>(
            entity, [&](AnimatorComponent &animator) { playing = animator.playing; });
        return playing;
    }

    bool AnimationController::IsCrossFading(fr::Entity entity) const
    {
        const auto *runtime = TryGetRuntime(entity);
        return runtime != nullptr && runtime->crossFading;
    }

    void AnimationController::SyncAnimGraph(fr::Entity entity, const AnimatorComponent &animator,
                                            const ModelAsset &model)
    {
        auto &runtime = EnsureRuntime(entity);
        if(!animator.useAnimGraph || animator.animGraph.states.empty())
        {
            runtime.animGraph.reset();
            runtime.graphFingerprint.clear();
            return;
        }

        const auto fingerprint = AnimGraphFingerprint(animator.animGraph, model.relativePath);
        if(runtime.animGraph && runtime.graphFingerprint == fingerprint)
        {
            return;
        }

        auto compiled = CompileAnimGraph(animator.animGraph, model);
        if(!compiled)
        {
            runtime.animGraph.reset();
            runtime.graphFingerprint.clear();
            return;
        }

        runtime.animGraph        = std::move(*compiled);
        runtime.graphFingerprint = fingerprint;

        for(const auto &[name, value] : runtime.floats)
        {
            runtime.animGraph->SetFloat(name, value);
        }
        for(const auto &[name, value] : runtime.bools)
        {
            runtime.animGraph->SetBool(name, value);
        }
        for(const auto &param : animator.animGraph.params)
        {
            if(param.kind == "Float" && !runtime.floats.contains(param.name))
            {
                runtime.floats[param.name] = param.defaultFloat;
                runtime.animGraph->SetFloat(param.name, param.defaultFloat);
            }
            else if(param.kind == "Bool" && !runtime.bools.contains(param.name))
            {
                runtime.bools[param.name] = param.defaultBool;
                runtime.animGraph->SetBool(param.name, param.defaultBool);
            }
        }
    }

    fra::AnimGraph *AnimationController::TryGetAnimGraph(fr::Entity entity)
    {
        auto *runtime = TryGetRuntime(entity);
        if(runtime == nullptr || !runtime->animGraph)
        {
            return nullptr;
        }
        return &*runtime->animGraph;
    }

} // namespace FRIGGA_NAMESPACE
