#pragma once

#include "Frigga/Audio/AudioTypes.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace FRIGGA_NAMESPACE
{

    class IAudioEngine
    {
      public:
        virtual ~IAudioEngine() = default;

        [[nodiscard]] virtual bool IsInitialized() const = 0;

        virtual bool Initialize() = 0;
        virtual void Update(float deltaTime) = 0;
        virtual void Shutdown() = 0;

        virtual bool LoadBank(const std::filesystem::path &absolutePath,
                              std::vector<std::string> &outEventPaths) = 0;
        virtual bool UnloadBank(const std::filesystem::path &absolutePath) = 0;
        [[nodiscard]] virtual std::vector<std::filesystem::path> GetLoadedBanks() const = 0;

        [[nodiscard]] virtual std::optional<AudioEventInfo> GetEventInfo(
            std::string_view eventPath) const = 0;

        [[nodiscard]] virtual AudioEventInstance CreateEventInstance(
            std::string_view eventPath) = 0;
        virtual void ReleaseEventInstance(AudioEventInstance instance) = 0;

        virtual bool StartEvent(AudioEventInstance instance) = 0;
        virtual void StopEvent(AudioEventInstance instance, bool allowFadeOut = true) = 0;
        virtual void PauseEvent(AudioEventInstance instance, bool paused) = 0;

        virtual void SetEventVolume(AudioEventInstance instance, float volume) = 0;
        virtual void SetEventPitch(AudioEventInstance instance, float pitch) = 0;
        virtual bool SetEventParameter(AudioEventInstance instance, std::string_view name,
                                       float value) = 0;
        virtual void SetEvent3DAttributes(AudioEventInstance instance, const glm::vec3 &position,
                                          const glm::vec3 &velocity) = 0;
        [[nodiscard]] virtual bool IsEventPlaying(AudioEventInstance instance) const = 0;

        virtual void SetListenerTransform(const glm::vec3 &position, const glm::quat &rotation) = 0;

        [[nodiscard]] virtual std::vector<AudioBusInfo> GetMixerBuses() const = 0;
        [[nodiscard]] virtual std::vector<AudioVcaInfo> GetMixerVcas() const = 0;
        virtual void SetBusVolume(std::string_view busPath, float volume) = 0;
        virtual void SetBusMute(std::string_view busPath, bool muted) = 0;
        virtual void SetVcaVolume(std::string_view vcaPath, float volume) = 0;

        [[nodiscard]] virtual std::optional<WaveformData> DecodeWaveform(
            const std::filesystem::path &absolutePath, int targetPeakCount = 2048) const = 0;
    };

} // namespace FRIGGA_NAMESPACE
