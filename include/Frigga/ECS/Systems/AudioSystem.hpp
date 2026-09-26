#pragma once

#include "Frigga/Audio/AudioController.hpp"
#include "Frigga/Audio/IAudioEngine.hpp"
#include "Frigga/Scene/Scene.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"

#include <Freyr/Freyr.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace FRIGGA_NAMESPACE
{

    class AudioSystem: public fr::System
    {
      public:
        AudioSystem(const skr::Arc<fr::Registry> &registry,
                    const skr::Arc<IAudioEngine> &audioEngine,
                    const skr::Arc<SceneSimulationState> &simulation,
                    const skr::Arc<Scene> &scene, const skr::Arc<AudioController> &controller);
        ~AudioSystem() override = default;

        void Update(float deltaTime) override;

      private:
        /// Last values pushed to the engine per entity; applySourceProperties only
        /// calls Set* when a value actually changed (miniaudio calls are not free).
        struct AppliedSourceProps
        {
            std::uint64_t instanceId = 0;
            float volume = 1.0f;
            float pitch  = 1.0f;
            bool  loop   = false;
            bool  is3D   = false;
            float minDistance = 1.0f;
            float maxDistance = 50.0f;
            std::unordered_map<std::string, float> parameters;
        };

        void releaseSource(fr::Entity entity, AudioSourceComponent &source);
        void stopAllSources();
        void applySourceProperties(fr::Entity entity, AudioSourceComponent &source);
        void syncListener();
        void syncSources();

        skr::Arc<IAudioEngine> mAudioEngine;
        skr::Arc<SceneSimulationState> mSimulation;
        skr::Arc<Scene> mScene;
        skr::Arc<AudioController> mController;
        bool mWasPlaying = false;
        std::unordered_map<fr::Entity, AppliedSourceProps> mAppliedProps;
    };

} // namespace FRIGGA_NAMESPACE
