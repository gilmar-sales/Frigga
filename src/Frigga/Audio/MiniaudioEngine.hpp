#pragma once

#include "Frigga/Macro.hpp"
#include "Frigga/Audio/IAudioEngine.hpp"

#include <Skirnir/Logging/Logger.hpp>
#include <Skirnir/Skirnir.hpp>

#include <filesystem>
#include <unordered_map>

namespace FRIGGA_NAMESPACE
{

    class MiniaudioEngine: public IAudioEngine
    {
      public:
        explicit MiniaudioEngine(const skr::Arc<skr::Logger<MiniaudioEngine>> &logger);
        ~MiniaudioEngine() override;

        [[nodiscard]] bool IsInitialized() const override
        {
            return mInitialized;
        }

        bool Initialize() override;
        void Update(float deltaTime) override;
        void Shutdown() override;

        bool LoadBank(const std::filesystem::path &absolutePath,
                      std::vector<std::string> &outEventPaths) override;
        bool UnloadBank(const std::filesystem::path &absolutePath) override;
        [[nodiscard]] std::vector<std::filesystem::path> GetLoadedBanks() const override;

        [[nodiscard]] std::optional<AudioEventInfo> GetEventInfo(
            std::string_view eventPath) const override;

        [[nodiscard]] AudioEventInstance CreateEventInstance(std::string_view eventPath) override;
        void ReleaseEventInstance(AudioEventInstance instance) override;

        bool StartEvent(AudioEventInstance instance) override;
        void StopEvent(AudioEventInstance instance, bool allowFadeOut = true) override;
        void PauseEvent(AudioEventInstance instance, bool paused) override;

        void SetEventVolume(AudioEventInstance instance, float volume) override;
        void SetEventPitch(AudioEventInstance instance, float pitch) override;
        bool SetEventParameter(AudioEventInstance instance, std::string_view name,
                               float value) override;
        void SetEvent3DAttributes(AudioEventInstance instance, const glm::vec3 &position,
                                  const glm::vec3 &velocity) override;
        [[nodiscard]] bool IsEventPlaying(AudioEventInstance instance) const override;

        void SetListenerTransform(const glm::vec3 &position,
                                  const glm::quat &rotation) override;

        [[nodiscard]] std::vector<AudioBusInfo> GetMixerBuses() const override;
        [[nodiscard]] std::vector<AudioVcaInfo> GetMixerVcas() const override;
        void SetBusVolume(std::string_view busPath, float volume) override;
        void SetBusMute(std::string_view busPath, bool muted) override;
        void SetVcaVolume(std::string_view vcaPath, float volume) override;

        [[nodiscard]] std::optional<WaveformData> DecodeWaveform(
            const std::filesystem::path &absolutePath, int targetPeakCount = 2048) const override;

      private:
        struct EventDef
        {
            std::filesystem::path clipAbsolute;
            float                 volume = 1.0f;
            float                 pitch  = 1.0f;
            bool                  loop   = false;
            std::string           bus    = "bus:/SFX";
        };

        struct InstanceEntry;

        [[nodiscard]] std::optional<EventDef> ResolveEvent(std::string_view eventPath) const;
        [[nodiscard]] float BusGain(std::string_view busPath) const;
        void ApplyBusGain(InstanceEntry &entry) const;
        [[nodiscard]] InstanceEntry *TryGetInstance(AudioEventInstance instance);
        [[nodiscard]] const InstanceEntry *TryGetInstance(AudioEventInstance instance) const;

        skr::Arc<skr::Logger<MiniaudioEngine>> mLogger;
        bool mInitialized = false;

        void *mEngine = nullptr;

        std::unordered_map<std::filesystem::path, std::vector<std::string>> mBankEvents;
        std::unordered_map<std::string, EventDef> mEvents;
        std::unordered_map<std::uint64_t, InstanceEntry> mInstances;
        std::unordered_map<std::string, float> mBusVolumes;
        std::unordered_map<std::string, bool> mBusMuted;
        std::uint64_t mNextInstanceId = 1;
    };

} // namespace FRIGGA_NAMESPACE
