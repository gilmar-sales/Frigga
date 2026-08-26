#pragma once

#include "Frigga/Audio/AudioController.hpp"
#include "Frigga/Audio/IAudioEngine.hpp"
#include "Frigga/Scene/Scene.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"

#include <Freyr/Freyr.hpp>

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
        void releaseSource(AudioSourceComponent &source);
        void stopAllSources();
        void applySourceProperties(AudioSourceComponent &source);
        void syncListener();
        void syncSources();

        skr::Arc<IAudioEngine> mAudioEngine;
        skr::Arc<SceneSimulationState> mSimulation;
        skr::Arc<Scene> mScene;
        skr::Arc<AudioController> mController;
        bool mWasPlaying = false;
    };

} // namespace FRIGGA_NAMESPACE
