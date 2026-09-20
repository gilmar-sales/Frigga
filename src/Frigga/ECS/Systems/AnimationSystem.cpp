#include <Frigga/ECS/Systems/AnimationSystem.hpp>

#include "Frigga/ECS/Components/AnimatorComponent.hpp"
#include "Frigga/ECS/Components/CameraComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"
#include "Frigga/ECS/TransformUtil.hpp"

#include <Freya/Advanced.hpp>
#include <Freya/Asset/AnimGraph.hpp>
#include <Freya/Asset/AnimationClip.hpp>
#include <Freya/Asset/Pose.hpp>
#include <Freya/Asset/Rig.hpp>
#include <Freya/FreyaOptions.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <unordered_set>
#include <utility>
#include <vector>

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        constexpr std::uint32_t kInvalidGpuClipSlot = 0xffffffffu;

        [[nodiscard]] std::uint64_t ClipGpuKey(const std::string &modelSource,
                                               std::string_view clipName)
        {
            return fra::GpuClipKey(modelSource + "/" + std::string(clipName));
        }

        void AdvanceClipTime(float &timeSec, float duration, float delta, bool loop)
        {
            if(duration <= 0.0f)
            {
                timeSec = 0.0f;
                return;
            }

            timeSec += delta;
            if(loop)
            {
                timeSec = std::fmod(timeSec, duration);
                if(timeSec < 0.0f)
                {
                    timeSec += duration;
                }
            }
            else
            {
                timeSec = std::clamp(timeSec, 0.0f, duration);
            }
        }

        void UploadSkeletonForModel(fra::GpuAnimationSystem &gpu, const ModelAsset &model)
        {
            gpu.UploadSkeleton(fra::PackSkeleton(model.skeleton));
            const auto root = fra::FindRootJoint(model.skeleton);
            gpu.SetRigIndices(0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu,
                              root >= 0 ? static_cast<std::uint32_t>(root) : 0u);
        }

        /// Freya 0.51+: FindClipSlot then EnsureClipResident on miss (thread-safe;
        /// free-slot fill only while instance staging is open).
        [[nodiscard]] std::uint32_t ResolveClipSlot(fra::GpuAnimationSystem &gpu,
                                                    const ModelAsset &model,
                                                    const fra::AnimationClip &clip)
        {
            const auto key = ClipGpuKey(model.relativePath, clip.name);
            const auto existing = gpu.FindClipSlot(key);
            if(existing != kInvalidGpuClipSlot)
            {
                return existing;
            }

            const auto *bake = model.BakedClipFor(clip);
            if(bake == nullptr || bake->Empty())
            {
                return kInvalidGpuClipSlot;
            }
            return gpu.EnsureClipResident(key, *bake);
        }

        [[nodiscard]] bool TryPackClipGpu(fra::GpuAnimInstance &instance,
                                          const ModelAsset &model,
                                          const fra::AnimationClip &clip, float timeSec,
                                          bool loop, std::uint32_t boneOffset,
                                          std::uint32_t jointCount, const glm::mat4 &modelWorld,
                                          fra::GpuAnimationSystem &gpu)
        {
            const auto slot = ResolveClipSlot(gpu, model, clip);
            if(slot == kInvalidGpuClipSlot)
            {
                return false;
            }

            instance            = {};
            instance.boneOffset = boneOffset;
            instance.jointCount = jointCount;
            instance.clipA      = slot;
            instance.timeA      = timeSec;
            instance.wA         = 1.0f;
            instance.flags      = loop ? fra::GpuAnimFlags::Loop : 0u;
            instance.modelWorld = modelWorld;
            return true;
        }

        [[nodiscard]] bool TryPackGraphGpu(
            fra::GpuAnimInstance &instance, fra::AnimGraph &graph, const ModelAsset &model,
            std::uint32_t boneOffset, std::uint32_t jointCount, const glm::mat4 &modelWorld,
            bool loop,
            const std::function<std::uint32_t(const fra::AnimationClip *)> &clipSlot)
        {
            fra::AnimLocoGpuSample loco {};
            if(!graph.TryGetLocoGpuSample(loco))
            {
                return false;
            }

            const auto slotA = clipSlot(loco.clipA);
            if(slotA == kInvalidGpuClipSlot)
            {
                return false;
            }

            const auto slotB = loco.clipB ? clipSlot(loco.clipB) : kInvalidGpuClipSlot;
            const auto slotC = loco.clipC ? clipSlot(loco.clipC) : kInvalidGpuClipSlot;

            instance            = {};
            instance.boneOffset = boneOffset;
            instance.jointCount = jointCount;
            instance.clipA      = slotA;
            instance.clipB      = slotB == kInvalidGpuClipSlot ? 0u : slotB;
            instance.clipC      = slotC == kInvalidGpuClipSlot ? 0u : slotC;
            instance.timeA      = loco.timeA;
            instance.timeB      = loco.timeB;
            instance.timeC      = loco.timeC;
            instance.wA         = loco.wA;
            instance.wB         = loco.wB;
            instance.wC         = loco.wC;
            instance.flags      = loop ? fra::GpuAnimFlags::Loop : 0u;
            instance.modelWorld = modelWorld;

            fra::AnimLayerGpuSlots layers {};
            if(graph.TryGetLayerGpuSlots(layers))
            {
                if(layers.masked.active && layers.masked.clip != nullptr)
                {
                    const auto maskSlot = clipSlot(layers.masked.clip);
                    if(maskSlot != kInvalidGpuClipSlot)
                    {
                        instance.clipMask   = maskSlot;
                        instance.timeMask   = layers.masked.time;
                        instance.weightMask = layers.masked.weight;
                        instance.flags |= fra::GpuAnimFlags::MaskedOverlay;
                    }
                }
                if(layers.additive.active && layers.additive.clip != nullptr)
                {
                    const auto addSlot = clipSlot(layers.additive.clip);
                    if(addSlot != kInvalidGpuClipSlot)
                    {
                        instance.clipAdd   = addSlot;
                        instance.timeAdd   = layers.additive.time;
                        instance.weightAdd = layers.additive.weight;
                        instance.flags |= fra::GpuAnimFlags::Additive;
                    }
                }
            }

            return true;
        }
    } // namespace

    AnimationSystem::AnimationSystem(const skr::Arc<fr::Registry> &registry,
                                     const skr::Arc<fra::Renderer> &renderer,
                                     const skr::Arc<AssetRegistry> &assets,
                                     const skr::Arc<Scene> &scene,
                                     const skr::Arc<SceneSimulationState> &simulation,
                                     const skr::Arc<fra::FreyaOptions> &options,
                                     const skr::Arc<AnimationController> &controller,
                                     const skr::Arc<AnimationEventRouter> &eventRouter)
        : System(registry), mRenderer(renderer), mAssets(assets), mScene(scene),
          mSimulation(simulation), mOptions(options), mController(controller),
          mEventRouter(eventRouter)
    {
    }

    const fra::AnimationClip *AnimationSystem::resolveClip(const ModelAsset &model,
                                                           const std::string &clipName) const
    {
        if(model.clips.empty())
        {
            return nullptr;
        }

        if(clipName.empty())
        {
            return &model.clips.front();
        }

        for(const auto &clip : model.clips)
        {
            if(clip.name == clipName)
            {
                return &clip;
            }
        }

        for(const auto &clip : model.clips)
        {
            if(clip.name.find(clipName) != std::string::npos)
            {
                return &clip;
            }
        }

        return &model.clips.front();
    }

    glm::vec3 AnimationSystem::cameraPosition() const
    {
        if(mScene->IsUsingEditorCamera())
        {
            return mScene->GetEditorCamera().transform.position;
        }
        if(mScene->IsUsingPreviewCamera())
        {
            return mScene->GetPreviewCamera().transform.position;
        }

        glm::vec3  fallback = mScene->GetEditorCamera().transform.position;
        fr::Entity primary  = static_cast<fr::Entity>(-1);
        fr::Entity first    = static_cast<fr::Entity>(-1);

        mRegistry->CreateMutation()->Each(
            [&](fr::Entity entity, TransformComponent &, CameraComponent &camera) {
                if(first == static_cast<fr::Entity>(-1))
                {
                    first = entity;
                }
                if(camera.primary)
                {
                    primary = entity;
                }
            });

        const fr::Entity chosen =
            primary != static_cast<fr::Entity>(-1) ? primary : first;
        if(chosen != static_cast<fr::Entity>(-1))
        {
            return glm::vec3(TransformUtil::WorldMatrix(*mRegistry, chosen)[3]);
        }

        return fallback;
    }

    bool AnimationSystem::consumeAnimationTick(float deltaTime, AnimatorComponent &animator,
                                               const glm::vec3 &actorPosition,
                                               const glm::vec3 &cameraPos, bool ticking,
                                               float &outAdvanceDt)
    {
        outAdvanceDt = 0.0f;
        if(!ticking)
        {
            return false;
        }
        if(!mOptions || !mOptions->enableAnimLod)
        {
            outAdvanceDt = deltaTime;
            return true;
        }

        const float dist = glm::length(actorPosition - cameraPos);
        fra::UpdateAnimLodTier(*mOptions, animator.lodTier, dist);
        const float hz = fra::AnimLodHz(*mOptions, animator.lodTier);
        if(!fra::ConsumeAnimLodTick(animator.lodAccum, deltaTime, hz))
        {
            return false;
        }

        outAdvanceDt = hz >= 1.0e5f ? deltaTime : (1.0f / std::max(hz, 1.0f));
        return true;
    }

    void AnimationSystem::ensureStableBoneOffset(AnimatorComponent &animator,
                                                 const ModelAsset &model)
    {
        const auto jointCount = model.skeleton.JointCount();
        if(jointCount == 0)
        {
            animator.boneOffset        = fra::kNoSkin;
            animator.boneCount         = 0;
            animator.bonePaletteSource.clear();
            return;
        }

        if(animator.boneOffset != fra::kNoSkin && animator.boneCount == jointCount &&
           animator.bonePaletteSource == model.relativePath)
        {
            return;
        }

        animator.bonePaletteSource = model.relativePath;
        animator.boneCount         = jointCount;
        animator.boneOffset =
            mNextBoneOffset.fetch_add(jointCount, std::memory_order_relaxed);
    }

    void AnimationSystem::pinGpuClipsForLoadedModels(fra::GpuAnimationSystem &gpu)
    {
        const auto skinned = mAssets->GetSkinnedModelsWithClips();
        if(skinned.empty())
        {
            mGpuPinnedModels.clear();
            mActiveGpuSkeletonPath.clear();
            return;
        }

        std::unordered_set<std::string> live;
        live.reserve(skinned.size());
        for(const auto *model : skinned)
        {
            if(model == nullptr || model->bakedClips.empty())
            {
                continue;
            }
            live.insert(model->relativePath);

            if(mGpuPinnedModels.insert(model->relativePath).second)
            {
                for(std::size_t i = 0; i < model->clips.size() && i < model->bakedClips.size();
                    ++i)
                {
                    const auto slot = gpu.EnsureClipResident(
                        ClipGpuKey(model->relativePath, model->clips[i].name),
                        model->bakedClips[i]);
                    if(slot != kInvalidGpuClipSlot)
                    {
                        gpu.PinClipSlot(slot, true);
                    }
                }
            }

            if(mActiveGpuSkeletonPath.empty())
            {
                UploadSkeletonForModel(gpu, *model);
                mActiveGpuSkeletonPath = model->relativePath;
            }
        }

        for(auto it = mGpuPinnedModels.begin(); it != mGpuPinnedModels.end();)
        {
            if(!live.contains(*it))
            {
                it = mGpuPinnedModels.erase(it);
            }
            else
            {
                ++it;
            }
        }

        if(!live.contains(mActiveGpuSkeletonPath))
        {
            mActiveGpuSkeletonPath.clear();
            if(const auto *model = skinned.front())
            {
                UploadSkeletonForModel(gpu, *model);
                mActiveGpuSkeletonPath = model->relativePath;
            }
        }
    }

    void AnimationSystem::enqueueEvents(fr::Entity entity,
                                        std::vector<fra::FiredAnimationEvent> &&events)
    {
        if(events.empty())
        {
            return;
        }
        mPendingEvents.emplace(entity, std::move(events));
    }

    void AnimationSystem::drainEvents()
    {
        if(!mEventRouter)
        {
            PendingAnimEvents discarded;
            while(mPendingEvents.try_pop(discarded))
            {
            }
            return;
        }

        PendingAnimEvents entry;
        while(mPendingEvents.try_pop(entry))
        {
            if(entry.events.empty() ||
               !mRegistry->HasComponent<AnimatorComponent>(entry.entity))
            {
                continue;
            }
            mRegistry->TryGetComponents<AnimatorComponent>(
                entry.entity, [&](AnimatorComponent &animator) {
                    mEventRouter->Dispatch(entry.entity, animator, entry.events);
                });
        }
    }

    void AnimationSystem::Update(float deltaTime)
    {
        const bool editMode = !mSimulation->IsPlaying();
        const bool animLod  = mOptions && mOptions->enableAnimLod;
        const glm::vec3 camPos = animLod ? cameraPosition() : glm::vec3(0.0f);

        auto &gpu = fra::Advanced(*mRenderer).GpuAnimation();
        const std::uint32_t gpuMaxJoints = gpu.GetJointsPerClipSlot();
        gpu.SetEnabled(false);
        mAnyGpuInstance.store(false, std::memory_order_relaxed);

        if(mController)
        {
            mController->PruneMissingAnimators();
        }

        pinGpuClipsForLoadedModels(gpu);

        // Must follow pin/UploadSkeleton — Freya forbids skeleton uploads while
        // instance staging is open. Application closes this session after RenderScene.
        gpu.BeginGpuAnimInstanceUploads();

        mRegistry->CreateMutation()->EachAsync(
            [&](fr::Entity entity, TransformComponent &, AnimatorComponent &animator) {
                if(animator.modelSource.empty())
                {
                    animator.boneOffset = fra::kNoSkin;
                    animator.boneCount  = 0;
                    return;
                }

                const auto *model = mAssets->FindModel(animator.modelSource);
                if(model == nullptr || !model->skinned || model->skeleton.JointCount() == 0)
                {
                    animator.boneOffset = fra::kNoSkin;
                    animator.boneCount  = 0;
                    return;
                }

                if(mController)
                {
                    mController->SyncAnimGraph(entity, animator, *model);
                }
                auto *graph = mController ? mController->TryGetAnimGraph(entity) : nullptr;

                const bool allowPreview = editMode && animator.previewInEdit;
                const bool ticking =
                    animator.playing && (allowPreview || mSimulation->IsRunning());

                const glm::mat4 modelWorld = TransformUtil::WorldMatrix(*mRegistry, entity);
                const glm::vec3 actorPos =
                    animLod ? glm::vec3(modelWorld[3]) : glm::vec3(0.0f);
                float      advanceDt = 0.0f;
                const bool mustEval =
                    consumeAnimationTick(deltaTime, animator, actorPos, camPos, ticking,
                                         advanceDt);

                ensureStableBoneOffset(animator, *model);
                const auto boneOffset = animator.boneOffset;
                const auto jointCount = animator.boneCount;
                if(boneOffset == fra::kNoSkin || jointCount == 0)
                {
                    return;
                }

                auto *runtime = mController ? mController->TryGetRuntime(entity) : nullptr;
                const bool crossFading = runtime != nullptr && runtime->crossFading;

                const auto clipSlotFn =
                    [&gpu, model](const fra::AnimationClip *clip) -> std::uint32_t {
                    if(clip == nullptr)
                    {
                        return kInvalidGpuClipSlot;
                    }
                    return ResolveClipSlot(gpu, *model, *clip);
                };

                const bool canGpu = animator.useGpu && !crossFading &&
                                    jointCount <= gpuMaxJoints &&
                                    model->relativePath == mActiveGpuSkeletonPath;

                auto uploadCpuSkin = [&](std::vector<glm::mat4> skin) {
                    if(skin.empty())
                    {
                        return;
                    }
                    mRenderer->UploadBoneMatrixUploads(boneOffset, skin);
                };

                auto tryUploadGpu = [&](fra::GpuAnimInstance &gpuInst) -> bool {
                    if(!canGpu)
                    {
                        return false;
                    }
                    gpu.UploadGpuAnimInstanceUploads(
                        std::span<const fra::GpuAnimInstance>(&gpuInst, 1));
                    mAnyGpuInstance.store(true, std::memory_order_relaxed);
                    return true;
                };

                if(graph != nullptr)
                {
                    std::vector<fra::FiredAnimationEvent> firedEvents;
                    if(mustEval && ticking)
                    {
                        if(runtime != nullptr)
                        {
                            for(const auto &[name, value] : runtime->floats)
                            {
                                graph->SetFloat(name, value);
                            }
                            for(const auto &[name, value] : runtime->bools)
                            {
                                graph->SetBool(name, value);
                            }
                            for(auto &[name, raised] : runtime->triggers)
                            {
                                if(raised)
                                {
                                    graph->SetTrigger(name);
                                    raised = false;
                                }
                            }
                        }
                        graph->Advance(advanceDt * animator.speed, &firedEvents);
                        animator.clipName = std::string {graph->CurrentStateName()};
                    }

                    enqueueEvents(entity, std::move(firedEvents));

                    fra::GpuAnimInstance gpuInst {};
                    if(canGpu &&
                       TryPackGraphGpu(gpuInst, *graph, *model, boneOffset, jointCount,
                                       modelWorld, animator.loop, clipSlotFn) &&
                       tryUploadGpu(gpuInst))
                    {
                        return;
                    }

                    if(mustEval || !canGpu)
                    {
                        uploadCpuSkin(
                            fra::PoseToSkinMatrices(model->skeleton, graph->SampleCurrent()));
                    }
                    return;
                }

                const auto *clip = resolveClip(*model, animator.clipName);
                if(clip == nullptr)
                {
                    return;
                }

                if(mustEval && ticking)
                {
                    const float tPrev = animator.clipTimePrev;
                    AdvanceClipTime(animator.timeSec, clip->duration,
                                    advanceDt * animator.speed, animator.loop);
                    std::vector<fra::FiredAnimationEvent> firedEvents;
                    fra::CollectFiredClipEvents(*clip, tPrev, animator.timeSec, animator.loop,
                                                firedEvents);
                    enqueueEvents(entity, std::move(firedEvents));
                    animator.clipTimePrev = animator.timeSec;

                    if(crossFading)
                    {
                        const auto *fromClip = resolveClip(*model, runtime->fromClip);
                        if(fromClip != nullptr)
                        {
                            AdvanceClipTime(runtime->fromTimeSec, fromClip->duration,
                                            advanceDt * animator.speed, animator.loop);
                        }
                        runtime->crossFadeElapsed += advanceDt;
                        if(runtime->crossFadeElapsed >= runtime->crossFadeDuration)
                        {
                            runtime->crossFading = false;
                        }
                    }
                }

                if(canGpu)
                {
                    fra::GpuAnimInstance gpuInst {};
                    if(TryPackClipGpu(gpuInst, *model, *clip, animator.timeSec, animator.loop,
                                      boneOffset, jointCount, modelWorld, gpu) &&
                       tryUploadGpu(gpuInst))
                    {
                        return;
                    }
                }

                std::vector<glm::mat4> skin;
                if(crossFading)
                {
                    const auto *fromClip = resolveClip(*model, runtime->fromClip);
                    if(fromClip != nullptr && runtime->crossFadeDuration > 0.0f)
                    {
                        const float t = std::clamp(
                            runtime->crossFadeElapsed / runtime->crossFadeDuration, 0.0f, 1.0f);
                        const auto fromPose =
                            fra::SampleClip(model->skeleton, *fromClip, runtime->fromTimeSec);
                        const auto toPose =
                            fra::SampleClip(model->skeleton, *clip, animator.timeSec);
                        const auto blended = fra::BlendLocalPoses(fromPose, toPose, t);
                        skin = fra::PoseToSkinMatrices(model->skeleton, blended);
                    }
                    else
                    {
                        skin = fra::EvaluateSkeletonPose(model->skeleton, *clip, animator.timeSec);
                        runtime->crossFading = false;
                    }
                }
                else if(mustEval || !canGpu)
                {
                    skin = fra::EvaluateSkeletonPose(model->skeleton, *clip, animator.timeSec);
                }
                else
                {
                    return;
                }

                uploadCpuSkin(std::move(skin));
            });
    }


    void AnimationSystem::PostUpdate(float)
    {
        drainEvents();
    }

    void AnimationSystem::CommitGpuAnimationFrame()
    {
        auto &gpu = fra::Advanced(*mRenderer).GpuAnimation();
        gpu.SetEnabled(mAnyGpuInstance.load(std::memory_order_relaxed));
        gpu.SetCopyPrevBones(true);
    }
} // namespace FRIGGA_NAMESPACE
