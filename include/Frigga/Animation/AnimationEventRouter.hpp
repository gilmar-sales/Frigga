#pragma once

#include "Frigga/Audio/AudioController.hpp"
#include "Frigga/ECS/Components/AnimatorComponent.hpp"
#include "Frigga/ECS/TransformUtil.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"

#include <Freya/Asset/AnimationClip.hpp>
#include <Freyr/Freyr.hpp>

#include <functional>
#include <span>
#include <string>
#include <vector>

namespace FRIGGA_NAMESPACE
{

    /// Routes Freya clip markers (Footstep.L, custom events) to audio and
    /// optional gameplay hooks while the simulation is running.
    class AnimationEventRouter
    {
      public:
        using GameplayHandler =
            std::function<void(fr::Entity, const fra::FiredAnimationEvent &)>;

        AnimationEventRouter(const skr::Arc<fr::Registry> &registry,
                             const skr::Arc<AudioController> &audio,
                             const skr::Arc<SceneSimulationState> &simulation);

        void SetGameplayHandler(GameplayHandler handler);

        void Dispatch(fr::Entity entity, const AnimatorComponent &animator,
                      std::span<const fra::FiredAnimationEvent> events);

      private:
        [[nodiscard]] std::string resolveAudioPath(const AnimatorComponent &animator,
                                                   std::string_view eventName) const;

        skr::Arc<fr::Registry>           mRegistry;
        skr::Arc<AudioController>        mAudio;
        skr::Arc<SceneSimulationState>   mSimulation;
        GameplayHandler                    mGameplayHandler;
    };

} // namespace FRIGGA_NAMESPACE
