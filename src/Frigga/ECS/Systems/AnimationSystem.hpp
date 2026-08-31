#pragma once

#include "Frigga/Animation/AnimationController.hpp"
#include "Frigga/Asset/AssetRegistry.hpp"
#include "Frigga/Scene/Scene.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"

#include <Freya/Asset/BakedAnimation.hpp>
#include <Freya/Asset/GpuAnimation.hpp>
#include <Freya/Freya.hpp>
#include <Freyr/Freyr.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>
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
                        const skr::Arc<AnimationController> &controller);

        ~AnimationSystem() override = default;

        void Update(float deltaTime) override;

      private:
        struct BakeKey
        {
            std::string modelSource;
            std::string clipName;

            bool operator==(const BakeKey &other) const
            {
                return modelSource == other.modelSource && clipName == other.clipName;
            }
        };

        struct BakeKeyHash
        {
            std::size_t operator()(const BakeKey &key) const noexcept
            {
                return std::hash<std::string> {}(key.modelSource) ^
                       (std::hash<std::string> {}(key.clipName) << 1);
            }
        };

        struct ActorLodState
        {
            float        accum = 0.f;
            std::uint8_t tier  = 0;
        };

        [[nodiscard]] const fra::AnimationClip *resolveClip(const ModelAsset &model,
                                                            const std::string &clipName) const;

        [[nodiscard]] const fra::BakedClip *ensureBake(const ModelAsset &model,
                                                       const fra::AnimationClip &clip);

        [[nodiscard]] glm::vec3 cameraPosition() const;

        /// Returns true when a pose/clip tick is due; writes wall-clock advance
        /// amount (LOD interval or frame delta) into @p outAdvanceDt.
        [[nodiscard]] bool consumeAnimationTick(float deltaTime, fr::Entity entity,
                                                const glm::vec3 &actorPosition, bool ticking,
                                                float &outAdvanceDt);

        skr::Arc<fra::Renderer> mRenderer;
        skr::Arc<AssetRegistry> mAssets;
        skr::Arc<Scene> mScene;
        skr::Arc<SceneSimulationState> mSimulation;
        skr::Arc<fra::FreyaOptions> mOptions;
        skr::Arc<AnimationController> mController;

        std::vector<glm::mat4> mBonePalette;
        std::vector<fra::GpuAnimInstance> mGpuInstances;
        std::unordered_map<BakeKey, fra::BakedClip, BakeKeyHash> mBakes;
        std::unordered_map<fr::Entity, ActorLodState> mLodStates;
    };

} // namespace FRIGGA_NAMESPACE
