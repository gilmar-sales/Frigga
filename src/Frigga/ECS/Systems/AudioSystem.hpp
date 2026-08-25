#pragma once

#include "Frigga/Audio/IAudioEngine.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"

#include <Freyr/Freyr.hpp>

namespace FRIGGA_NAMESPACE
{

    class AudioSystem: public fr::System
    {
      public:
        AudioSystem(const skr::Arc<fr::Registry> &registry,
                    const skr::Arc<IAudioEngine> &audioEngine,
                    const skr::Arc<SceneSimulationState> &simulation);
        ~AudioSystem() override = default;

        void Update(float deltaTime) override;

      private:
        void syncListener();
        void syncSources(float deltaTime);
        void stopAllSources();

        skr::Arc<IAudioEngine> mAudioEngine;
        skr::Arc<SceneSimulationState> mSimulation;
        bool mWasPlaying = false;
    };

} // namespace FRIGGA_NAMESPACE
