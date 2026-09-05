#pragma once

#include "Frigga/Audio/IAudioEngine.hpp"
#include "Frigga/ECS/Components/AudioSourceComponent.hpp"

#include <Freyr/Freyr.hpp>

#include <glm/glm.hpp>
#include <string_view>
#include <vector>

namespace FRIGGA_NAMESPACE
{

    /**
     * @brief Gameplay-facing audio API (Play / Stop / params).
     *
     * Mutates AudioSourceComponent intents; AudioSystem syncs to IAudioEngine in Play.
     * PreviewEvent is the only direct engine path (edit-mode tooling).
     */
    class AudioController
    {
      public:
        AudioController(const skr::Arc<fr::Registry> &registry,
                        const skr::Arc<IAudioEngine> &audioEngine);

        bool Play(fr::Entity entity, std::string_view eventPath = {}, bool oneShot = false);
        void Stop(fr::Entity entity);
        void Pause(fr::Entity entity);
        void Resume(fr::Entity entity);

        void SetVolume(fr::Entity entity, float volume);
        void SetPitch(fr::Entity entity, float pitch);
        bool SetParameter(fr::Entity entity, std::string_view name, float value);
        void SetGlobalParameter(std::string_view name, float value);

        [[nodiscard]] bool IsPlaying(fr::Entity entity) const;

        /// Preview in edit mode (does not require play session).
        bool PreviewEvent(std::string_view eventPath, float volume = 1.0f, bool loop = false);
        void StopPreview();

        /// One-shot 3D clip at a world position (no AudioSourceComponent required).
        bool PlayOneShotAt(std::string_view eventPath, const glm::vec3 &position,
                           float volume = 1.0f);

        /// Prune finished one-shot voices. Call from AudioSystem each frame.
        void UpdateOneShots();

        /// Stops preview and every AudioSource engine instance (e.g. on Play session exit).
        void StopAllPlayback();

      private:
        struct PreviewRuntime
        {
            AudioEventInstance instance {};
        };

        struct OneShotVoice
        {
            AudioEventInstance instance {};
            glm::vec3          position {0.0f};
        };

        skr::Arc<fr::Registry> mRegistry;
        skr::Arc<IAudioEngine> mAudioEngine;
        PreviewRuntime mPreview;
        std::vector<OneShotVoice> mOneShots;
    };

} // namespace FRIGGA_NAMESPACE
