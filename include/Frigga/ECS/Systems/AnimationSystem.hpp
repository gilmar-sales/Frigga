#pragma once

#include "Frigga/Animation/AnimationController.hpp"
#include "Frigga/Animation/AnimationEventRouter.hpp"
#include "Frigga/Asset/AssetRegistry.hpp"
#include "Frigga/Scene/Scene.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"

#include <Freya/Advanced.hpp>
#include <Freya/Asset/BakedAnimation.hpp>
#include <Freya/Asset/GpuAnimation.hpp>
#include <Freya/Asset/AnimationClip.hpp>
#include <Freyr/Containers/UnboundedMPMCQueue.hpp>
#include <Freyr/Freyr.hpp>
#include <glm/glm.hpp>

#include <atomic>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace FRIGGA_NAMESPACE
{

    class AnimationSystem: public fr::System
    {
      public:
        AnimationSystem(const skr::Arc<fr::Registry> &registry,
                        const skr::Arc<fra::Renderer> &renderer,
                        const skr::Arc<AssetRegistry> &assets,
                        const skr::Arc<Scene> &scene,
                        const skr::Arc<SceneSimulationState> &simulation,
                        const skr::Arc<fra::FreyaOptions> &options,
                        const skr::Arc<AnimationController> &controller,
                        const skr::Arc<AnimationEventRouter> &eventRouter);

        ~AnimationSystem() override = default;

        /// Evaluates poses then drains clip events (Render pipeline, after HierarchyPropagation).
        void PostUpdate(float deltaTime) override;

        /// After Application closes bone/GPU-anim upload sessions for the frame.
        void CommitGpuAnimationFrame();

      private:
        void evaluate(float deltaTime);

        /// Must be nothrow move-constructible for rigtorp::UnboundedMPMCQueue.
        struct PendingAnimEvents
        {
            fr::Entity                                entity = static_cast<fr::Entity>(-1);
            std::vector<fra::FiredAnimationEvent> events;

            PendingAnimEvents() noexcept = default;

            PendingAnimEvents(fr::Entity e,
                              std::vector<fra::FiredAnimationEvent> &&ev) noexcept
                : entity(e), events(std::move(ev))
            {
            }

            PendingAnimEvents(PendingAnimEvents &&other) noexcept = default;
            PendingAnimEvents &operator=(PendingAnimEvents &&other) noexcept = default;

            PendingAnimEvents(const PendingAnimEvents &)            = delete;
            PendingAnimEvents &operator=(const PendingAnimEvents &) = delete;
        };

        [[nodiscard]] const fra::AnimationClip *resolveClip(const ModelAsset &model,
                                                            const std::string &clipName) const;

        [[nodiscard]] glm::vec3 cameraPosition() const;

        /// Returns true when a pose/clip tick is due; writes wall-clock advance
        /// amount (LOD interval or frame delta) into @p outAdvanceDt.
        [[nodiscard]] bool consumeAnimationTick(float deltaTime, AnimatorComponent &animator,
                                                const glm::vec3 &actorPosition,
                                                const glm::vec3 &cameraPos, bool ticking,
                                                float &outAdvanceDt);

        void ensureStableBoneOffset(AnimatorComponent &animator, const ModelAsset &model);

        /// Pin loaded model clips + skeleton atlas slabs (Ensure*Resident; multi-rig).
        void pinGpuClipsForLoadedModels(fra::GpuAnimationSystem &gpu);

        void enqueueEvents(fr::Entity entity,
                           std::vector<fra::FiredAnimationEvent> &&events);
        void drainEvents();

        skr::Arc<fra::Renderer> mRenderer;
        skr::Arc<AssetRegistry> mAssets;
        skr::Arc<Scene> mScene;
        skr::Arc<SceneSimulationState> mSimulation;
        skr::Arc<fra::FreyaOptions> mOptions;
        skr::Arc<AnimationController> mController;
        skr::Arc<AnimationEventRouter> mEventRouter;

        std::atomic<std::uint32_t> mNextBoneOffset {0};
        std::unordered_set<std::string> mGpuPinnedModels;
        std::atomic<bool> mAnyGpuInstance {false};

        rigtorp::UnboundedMPMCQueue<PendingAnimEvents> mPendingEvents;
    };

} // namespace FRIGGA_NAMESPACE
