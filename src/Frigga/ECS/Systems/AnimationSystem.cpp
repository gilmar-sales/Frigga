#include "AnimationSystem.hpp"

#include "Frigga/ECS/Components/AnimatorComponent.hpp"
#include "Frigga/ECS/Components/MeshComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        constexpr std::uint32_t kInvalidGpuClipSlot = 0xffffffffu;

        glm::mat4 BuildModelMatrix(const TransformComponent &transform)
        {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), transform.position);
            model           = model * glm::mat4_cast(transform.rotation);
            model           = glm::scale(model, transform.scale);
            return model;
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
    } // namespace

    AnimationSystem::AnimationSystem(const skr::Arc<fr::Registry> &registry,
                                     const skr::Arc<fra::Renderer> &renderer,
                                     const skr::Arc<AssetRegistry> &assets,
                                     const skr::Arc<SceneSimulationState> &simulation,
                                     const skr::Arc<fra::FreyaOptions> &options)
        : System(registry), mRenderer(renderer), mAssets(assets), mSimulation(simulation),
          mOptions(options)
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

    void AnimationSystem::Update(float deltaTime)
    {
        mBonePalette.clear();
        mGpuInstances.clear();

        const bool editMode = !mSimulation->IsPlaying();

        auto *gpuPass = mRenderer->GetGpuAnimPass();
        if(gpuPass != nullptr)
        {
            gpuPass->SetEnabled(false);
        }

        std::unordered_map<std::string, std::uint32_t> gpuVotes;
        mRegistry->CreateMutation()->Each<AnimatorComponent>(
            [&](auto, AnimatorComponent &animator) {
                animator.boneOffset = fra::kNoSkin;
                animator.boneCount  = 0;
                if(!animator.useGpu || animator.modelSource.empty())
                {
                    return;
                }
                const auto *model = mAssets->FindModel(animator.modelSource);
                if(model != nullptr && model->skinned && model->skeleton.JointCount() > 0)
                {
                    ++gpuVotes[animator.modelSource];
                }
            });

        std::string gpuPreferredSource;
        std::uint32_t gpuPreferredCount = 0;
        for(const auto &[source, count] : gpuVotes)
        {
            if(count > gpuPreferredCount)
            {
                gpuPreferredSource = source;
                gpuPreferredCount  = count;
            }
        }

        const ModelAsset *gpuModel = nullptr;
        if(gpuPass != nullptr && gpuPreferredCount > 0)
        {
            gpuModel = mAssets->FindModel(gpuPreferredSource);
            if(gpuModel == nullptr || !gpuModel->skinned)
            {
                gpuModel = nullptr;
            }
        }

        if(gpuModel != nullptr && gpuPass != nullptr)
        {
            if(!mGpuSkeletonUploaded || mGpuSkeletonSource != gpuModel->relativePath)
            {
                gpuPass->UploadSkeleton(fra::PackSkeleton(gpuModel->skeleton));
                const auto root = fra::FindRootJoint(gpuModel->skeleton);
                gpuPass->SetRigIndices(0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu,
                                       root >= 0 ? static_cast<std::uint32_t>(root) : 0u);
                mGpuSkeletonSource   = gpuModel->relativePath;
                mGpuSkeletonUploaded = true;
            }
            gpuPass->SetCopyPrevBones(true);
        }

        mRegistry->CreateMutation()->Each<TransformComponent, MeshComponent, AnimatorComponent>(
            [&](auto, TransformComponent &transform, MeshComponent &,
                AnimatorComponent &animator) {
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

                const auto *clip = resolveClip(*model, animator.clipName);
                if(clip == nullptr)
                {
                    return;
                }

                const bool allowPreview = editMode && animator.previewInEdit;
                if(animator.playing && (allowPreview || mSimulation->IsRunning()))
                {
                    AdvanceClipTime(animator.timeSec, clip->duration, deltaTime * animator.speed,
                                    animator.loop);
                }

                const auto jointCount = model->skeleton.JointCount();
                const auto boneOffset = static_cast<std::uint32_t>(mBonePalette.size());
                mBonePalette.resize(mBonePalette.size() + jointCount, glm::mat4(1.0f));

                animator.boneOffset = boneOffset;
                animator.boneCount  = jointCount;

                const bool useGpuThisActor =
                    animator.useGpu && gpuModel != nullptr &&
                    model->relativePath == gpuModel->relativePath && gpuPass != nullptr &&
                    jointCount <= fra::GpuAnimPass::kMaxJoints &&
                    mGpuInstances.size() < fra::GpuAnimPass::kMaxInstances;

                if(useGpuThisActor)
                {
                    const auto *bake = ensureBake(*model, *clip);
                    const auto slot =
                        gpuPass->EnsureClipResident(fra::GpuClipKey(clip->name), *bake);
                    if(slot != kInvalidGpuClipSlot)
                    {
                        fra::GpuAnimInstance instance {};
                        instance.boneOffset = boneOffset;
                        instance.jointCount = jointCount;
                        instance.clipA      = slot;
                        instance.timeA      = animator.timeSec;
                        instance.wA         = 1.0f;
                        instance.flags = animator.loop ? fra::GpuAnimFlags::Loop : 0u;
                        instance.modelWorld = BuildModelMatrix(transform);
                        mGpuInstances.push_back(instance);
                        return;
                    }
                }

                const auto skin =
                    fra::EvaluateSkeletonPose(model->skeleton, *clip, animator.timeSec);
                std::copy(skin.begin(), skin.end(),
                          mBonePalette.begin() + static_cast<std::ptrdiff_t>(boneOffset));
            });

        if(!mBonePalette.empty())
        {
            mRenderer->UploadBoneMatrices(mBonePalette);
        }

        if(gpuPass != nullptr)
        {
            if(!mGpuInstances.empty())
            {
                gpuPass->UploadInstances(mGpuInstances);
                gpuPass->SetEnabled(true);
                mRenderer->SetGpuAnimEnabled(true);
            }
            else
            {
                gpuPass->SetEnabled(false);
                mRenderer->SetGpuAnimEnabled(false);
            }
        }
    }

} // namespace FRIGGA_NAMESPACE
