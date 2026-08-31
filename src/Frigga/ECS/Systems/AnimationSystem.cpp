#include "AnimationSystem.hpp"

#include "Frigga/ECS/Components/AnimatorComponent.hpp"
#include "Frigga/ECS/Components/CameraComponent.hpp"
#include "Frigga/ECS/Components/MeshComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"
#include "Frigga/ECS/TransformUtil.hpp"

#include <Freya/Asset/AnimGraph.hpp>
#include <Freya/Asset/Pose.hpp>
#include <Freya/Asset/Rig.hpp>
#include <Freya/FreyaOptions.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <unordered_map>
#include <vector>

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        constexpr std::uint32_t kInvalidGpuClipSlot  = 0xffffffffu;
        constexpr std::uint32_t kMaxGpuAnimInstances = 2048u;

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

        void UploadSkeletonForModel(fra::Renderer &renderer, const ModelAsset &model)
        {
            renderer.UploadGpuAnimSkeleton(fra::PackSkeleton(model.skeleton));
            const auto root = fra::FindRootJoint(model.skeleton);
            renderer.SetGpuAnimRigIndices(
                0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu,
                root >= 0 ? static_cast<std::uint32_t>(root) : 0u);
        }

        [[nodiscard]] bool TryPackClipGpu(fra::GpuAnimInstance &instance,
                                          const ModelAsset &model,
                                          const fra::AnimationClip &clip, float timeSec,
                                          bool loop, std::uint32_t boneOffset,
                                          std::uint32_t jointCount, const glm::mat4 &modelWorld,
                                          fra::Renderer &renderer, const fra::BakedClip &bake)
        {
            const auto slot = renderer.EnsureGpuAnimClipResident(
                ClipGpuKey(model.relativePath, clip.name), bake);
            if(slot == kInvalidGpuClipSlot)
            {
                return false;
            }

            instance                 = {};
            instance.boneOffset      = boneOffset;
            instance.jointCount      = jointCount;
            instance.clipA           = slot;
            instance.timeA           = timeSec;
            instance.wA              = 1.0f;
            instance.flags           = loop ? fra::GpuAnimFlags::Loop : 0u;
            instance.modelWorld      = modelWorld;
            return true;
        }

        [[nodiscard]] bool TryPackGraphGpu(
            fra::GpuAnimInstance &instance, fra::AnimGraph &graph, const ModelAsset &model,
            std::uint32_t boneOffset, std::uint32_t jointCount, const glm::mat4 &modelWorld,
            bool loop, fra::Renderer &renderer,
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
                                     const skr::Arc<AnimationController> &controller)
        : System(registry), mRenderer(renderer), mAssets(assets), mScene(scene),
          mSimulation(simulation), mOptions(options), mController(controller)
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

    const fra::BakedClip *AnimationSystem::ensureBake(const ModelAsset &model,
                                                      const fra::AnimationClip &clip)
    {
        BakeKey key {.modelSource = model.relativePath, .clipName = clip.name};
        if(const auto it = mBakes.find(key); it != mBakes.end())
        {
            return &it->second;
        }

        const float bakeHz = mOptions ? std::max(mOptions->animBakeHz, 1.0f) : 30.0f;
        auto [it, inserted] =
            mBakes.emplace(std::move(key), fra::BakeClip(model.skeleton, clip, bakeHz));
        (void)inserted;
        return &it->second;
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

        glm::vec3     position = mScene->GetEditorCamera().transform.position;
        bool          found    = false;
        fr::Entity    primary  = static_cast<fr::Entity>(-1);

        mRegistry->CreateMutation()->Each(
            [&](fr::Entity entity, TransformComponent &, CameraComponent &camera) {
                if(camera.primary)
                {
                    primary = entity;
                }
            });

        if(primary != static_cast<fr::Entity>(-1))
        {
            return TransformUtil::WorldPose(*mRegistry, primary).position;
        }

        mRegistry->CreateMutation()->Each(
            [&](fr::Entity entity, TransformComponent &, CameraComponent &) {
                if(!found)
                {
                    position = TransformUtil::WorldPose(*mRegistry, entity).position;
                    found    = true;
                }
            });

        return position;
    }

    bool AnimationSystem::consumeAnimationTick(float deltaTime, fr::Entity entity,
                                               const glm::vec3 &actorPosition, bool ticking,
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

        auto &lod = mLodStates[entity];
        const float dist = glm::length(actorPosition - cameraPosition());
        fra::UpdateAnimLodTier(*mOptions, lod.tier, dist);
        const float hz = fra::AnimLodHz(*mOptions, lod.tier);
        if(!fra::ConsumeAnimLodTick(lod.accum, deltaTime, hz))
        {
            return false;
        }

        outAdvanceDt = hz >= 1.0e5f ? deltaTime : (1.0f / std::max(hz, 1.0f));
        return true;
    }

    void AnimationSystem::Update(float deltaTime)
    {
        mBonePalette.clear();
        mGpuInstances.clear();

        const bool editMode = !mSimulation->IsPlaying();

        const std::uint32_t gpuMaxJoints = mRenderer->GetGpuAnimJointsPerClipSlot();
        mRenderer->SetGpuAnimEnabled(false);

        std::unordered_map<std::string, std::vector<fra::GpuAnimInstance>> gpuBatches;
        std::unordered_map<std::string, const ModelAsset *> gpuBatchModels;

        mRegistry->CreateMutation()->Each(
            [&](fr::Entity entity, TransformComponent &, MeshComponent &,
                AnimatorComponent &animator) {
                animator.boneOffset = fra::kNoSkin;
                animator.boneCount  = 0;

                if(animator.modelSource.empty())
                {
                    return;
                }

                const auto *model = mAssets->FindModel(animator.modelSource);
                if(model == nullptr)
                {
                    (void)mAssets->LoadModel(animator.modelSource);
                    model = mAssets->FindModel(animator.modelSource);
                }
                if(model == nullptr || !model->skinned || model->skeleton.JointCount() == 0)
                {
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

                const auto worldPose = TransformUtil::WorldPose(*mRegistry, entity);
                const auto modelWorld = TransformUtil::WorldMatrix(*mRegistry, entity);
                float      advanceDt = 0.0f;
                const bool mustEval =
                    consumeAnimationTick(deltaTime, entity, worldPose.position, ticking,
                                         advanceDt);

                const auto jointCount = model->skeleton.JointCount();
                const auto boneOffset = static_cast<std::uint32_t>(mBonePalette.size());
                mBonePalette.resize(mBonePalette.size() + jointCount, glm::mat4(1.0f));
                animator.boneOffset = boneOffset;
                animator.boneCount  = jointCount;

                auto *runtime = mController ? mController->TryGetRuntime(entity) : nullptr;
                const bool crossFading = runtime != nullptr && runtime->crossFading;

                const auto clipSlotFn =
                    [this, model](const fra::AnimationClip *clip) -> std::uint32_t {
                    if(clip == nullptr)
                    {
                        return kInvalidGpuClipSlot;
                    }
                    const auto *bake = ensureBake(*model, *clip);
                    return mRenderer->EnsureGpuAnimClipResident(
                        ClipGpuKey(model->relativePath, clip->name), *bake);
                };

                const bool canGpu =
                    animator.useGpu && !crossFading && jointCount <= gpuMaxJoints;

                auto tryQueueGpu = [&](fra::GpuAnimInstance &gpuInst) -> bool {
                    if(!canGpu)
                    {
                        return false;
                    }
                    auto &batch = gpuBatches[model->relativePath];
                    if(batch.size() >= kMaxGpuAnimInstances)
                    {
                        return false;
                    }
                    batch.push_back(gpuInst);
                    gpuBatchModels[model->relativePath] = model;
                    return true;
                };

                if(graph != nullptr)
                {
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
                        graph->Advance(advanceDt * animator.speed);
                        animator.clipName = std::string {graph->CurrentStateName()};
                    }

                    fra::GpuAnimInstance gpuInst {};
                    if(canGpu &&
                       TryPackGraphGpu(gpuInst, *graph, *model, boneOffset, jointCount,
                                       modelWorld, animator.loop, *mRenderer, clipSlotFn) &&
                       tryQueueGpu(gpuInst))
                    {
                        return;
                    }

                    if(mustEval || !canGpu)
                    {
                        const auto pose = graph->SampleCurrent();
                        const auto skin = fra::PoseToSkinMatrices(model->skeleton, pose);
                        std::copy(skin.begin(), skin.end(),
                                  mBonePalette.begin() +
                                      static_cast<std::ptrdiff_t>(boneOffset));
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
                    AdvanceClipTime(animator.timeSec, clip->duration,
                                    advanceDt * animator.speed, animator.loop);
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
                    const auto *bake = ensureBake(*model, *clip);
                    fra::GpuAnimInstance gpuInst {};
                    if(TryPackClipGpu(gpuInst, *model, *clip, animator.timeSec, animator.loop,
                                      boneOffset, jointCount, modelWorld, *mRenderer, *bake) &&
                       tryQueueGpu(gpuInst))
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

                std::copy(skin.begin(), skin.end(),
                          mBonePalette.begin() + static_cast<std::ptrdiff_t>(boneOffset));
            });

        if(!mBonePalette.empty())
        {
            mRenderer->UploadBoneMatrices(mBonePalette);
        }

        if(gpuBatches.empty())
        {
            mRenderer->SetGpuAnimEnabled(false);
            return;
        }

        std::vector<std::string> batchOrder;
        batchOrder.reserve(gpuBatches.size());
        for(const auto &[source, batch] : gpuBatches)
        {
            (void)batch;
            batchOrder.push_back(source);
        }
        std::sort(batchOrder.begin(), batchOrder.end(),
                  [&](const std::string &a, const std::string &b) {
                      return gpuBatches[a].size() > gpuBatches[b].size();
                  });

        const auto frameIndex = mRenderer->GetCurrentFrameIndex();

        for(std::size_t i = 0; i < batchOrder.size(); ++i)
        {
            const auto &source = batchOrder[i];
            const auto *model  = gpuBatchModels[source];
            if(model == nullptr)
            {
                continue;
            }

            UploadSkeletonForModel(*mRenderer, *model);

            auto &batch = gpuBatches[source];
            if(i == 0)
            {
                mGpuInstances = std::move(batch);
                mRenderer->SetGpuAnimCopyPrevBones(true);
                mRenderer->UploadGpuAnimInstances(mGpuInstances);
                mRenderer->SetGpuAnimEnabled(true);
            }
            else
            {
                mRenderer->SetGpuAnimCopyPrevBones(false);
                (void)mRenderer->DispatchGpuAnimImmediate(batch, frameIndex);
            }
        }
    }

} // namespace FRIGGA_NAMESPACE
