#include <Frigga/Animation/AnimationController.hpp>

#include "Frigga/Animation/AnimGraphDefinition.hpp"

#include <algorithm>
#include <bit>
#include <mutex>

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        constexpr std::uint64_t kFnvOffset = 14695981039346656037ull;
        constexpr std::uint64_t kFnvPrime  = 1099511628211ull;

        void HashBytes(std::uint64_t &hash, const void *data, std::size_t size)
        {
            const auto *bytes = static_cast<const unsigned char *>(data);
            for(std::size_t i = 0; i < size; ++i)
            {
                hash ^= bytes[i];
                hash *= kFnvPrime;
            }
        }

        void HashString(std::uint64_t &hash, std::string_view text)
        {
            HashBytes(hash, text.data(), text.size());
            HashBytes(hash, "|", 1);
        }

        void HashFloat(std::uint64_t &hash, float value)
        {
            const auto bits = std::bit_cast<std::uint32_t>(value);
            HashBytes(hash, &bits, sizeof(bits));
        }

        /// Allocation-free content hash mirroring AnimGraphFingerprint inputs.
        [[nodiscard]] std::uint64_t HashAnimGraph(const AnimGraphDefinition &graph,
                                                 std::string_view modelSource)
        {
            std::uint64_t hash = kFnvOffset;
            HashString(hash, modelSource);
            HashString(hash, graph.entry);
            for(const auto &param : graph.params)
            {
                HashString(hash, param.name);
                HashString(hash, param.kind);
                HashFloat(hash, param.defaultFloat);
                HashBytes(hash, &param.defaultBool, sizeof(param.defaultBool));
                HashFloat(hash, param.minValue);
                HashFloat(hash, param.maxValue);
                HashBytes(hash, &param.hasRange, sizeof(param.hasRange));
            }
            for(const auto &state : graph.states)
            {
                HashString(hash, state.name);
                HashString(hash, state.kind);
                HashString(hash, state.clip);
                HashString(hash, state.blendParam);
                HashString(hash, state.blendParamY);
                HashBytes(hash, &state.loop, sizeof(state.loop));
                HashBytes(hash, &state.syncPhase, sizeof(state.syncPhase));
                HashFloat(hash, state.playbackSpeed);
                for(const auto &sample : state.blendSamples)
                {
                    HashString(hash, sample.clip);
                    HashFloat(hash, sample.value);
                    HashFloat(hash, sample.x);
                    HashFloat(hash, sample.y);
                    HashBytes(hash, &sample.loop, sizeof(sample.loop));
                    HashFloat(hash, sample.playbackSpeed);
                }
            }
            for(const auto &transition : graph.transitions)
            {
                HashString(hash, transition.from);
                HashString(hash, transition.to);
                HashString(hash, transition.conditionKind);
                HashString(hash, transition.param);
                HashFloat(hash, transition.threshold);
                HashFloat(hash, transition.blendDuration);
            }
            return hash;
        }
    } // namespace

    namespace
    {
        AnimationController::EntityRuntime &EnsureRuntimeLocked(
            std::unordered_map<fr::Entity, std::unique_ptr<AnimationController::EntityRuntime>>
                &runtimes,
            fr::Entity entity)
        {
            auto &slot = runtimes[entity];
            if(!slot)
            {
                slot = std::make_unique<AnimationController::EntityRuntime>();
            }
            return *slot;
        }
    } // namespace

    AnimationController::AnimationController(const skr::Arc<fr::Registry> &registry,
                                             const skr::Arc<AssetRegistry> &assets)
        : mRegistry(registry), mAssets(assets)
    {
    }

    AnimationController::EntityRuntime &AnimationController::EnsureRuntime(fr::Entity entity)
    {
        std::lock_guard lock(mRuntimesMutex);
        return EnsureRuntimeLocked(mRuntimes, entity);
    }

    AnimationController::EntityRuntime *AnimationController::TryGetRuntime(fr::Entity entity)
    {
        std::lock_guard lock(mRuntimesMutex);
        const auto it = mRuntimes.find(entity);
        return it == mRuntimes.end() || !it->second ? nullptr : it->second.get();
    }

    const AnimationController::EntityRuntime *AnimationController::TryGetRuntime(
        fr::Entity entity) const
    {
        std::lock_guard lock(mRuntimesMutex);
        const auto it = mRuntimes.find(entity);
        return it == mRuntimes.end() || !it->second ? nullptr : it->second.get();
    }

    void AnimationController::ClearRuntime(fr::Entity entity)
    {
        std::lock_guard lock(mRuntimesMutex);
        mRuntimes.erase(entity);
    }

    void AnimationController::PruneMissingAnimators()
    {
        std::lock_guard lock(mRuntimesMutex);
        for(auto it = mRuntimes.begin(); it != mRuntimes.end();)
        {
            if(!mRegistry->HasComponent<AnimatorComponent>(it->first))
            {
                it = mRuntimes.erase(it);
            }
            else
            {
                ++it;
            }
        }
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
        std::lock_guard lock(mRuntimesMutex);
        auto &runtime = EnsureRuntimeLocked(mRuntimes, entity);
        if(!animator.useAnimGraph || animator.animGraph.states.empty())
        {
            runtime.animGraph.reset();
            runtime.graphFingerprint.clear();
            runtime.graphContentHash = 0;
            return;
        }

        // Content hash first (no allocation): the full fingerprint below builds
        // strings with std::to_string per state/transition every frame.
        const std::uint64_t contentHash = HashAnimGraph(animator.animGraph, model.relativePath);
        if(runtime.animGraph && runtime.graphContentHash == contentHash)
        {
            return;
        }

        const auto fingerprint = AnimGraphFingerprint(animator.animGraph, model.relativePath);
        if(runtime.animGraph && runtime.graphFingerprint == fingerprint)
        {
            runtime.graphContentHash = contentHash;
            return;
        }

        auto compiled = CompileAnimGraph(animator.animGraph, model);
        if(!compiled)
        {
            runtime.animGraph.reset();
            runtime.graphFingerprint.clear();
            runtime.graphContentHash = contentHash;
            return;
        }

        runtime.animGraph        = std::move(*compiled);
        runtime.graphFingerprint = fingerprint;
        runtime.graphContentHash = contentHash;

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
