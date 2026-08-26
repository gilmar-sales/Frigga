#include "MiniaudioEngine.hpp"

#include "Frigga/Asset/AssetRegistry.hpp"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#define SIMDJSON_STATIC_REFLECTION 1
#include <simdjson.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>
#include <sstream>

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        struct AudioBankEventDto
        {
            std::string          path;
            std::string          clip;
            std::optional<float> volume;
            std::optional<float> pitch;
            std::optional<bool>  loop;
            std::optional<std::string> bus;
        };

        struct AudioBankDto
        {
            std::vector<AudioBankEventDto> events;
        };

        glm::vec3 ForwardFromQuat(const glm::quat &rotation)
        {
            return rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        }

        glm::vec3 UpFromQuat(const glm::quat &rotation)
        {
            return rotation * glm::vec3(0.0f, 1.0f, 0.0f);
        }

        [[nodiscard]] std::filesystem::path NormalizeBankKey(const std::filesystem::path &path)
        {
            std::error_code ec;
            return std::filesystem::weakly_canonical(path, ec).generic_string();
        }

        [[nodiscard]] bool IsDirectClipPath(std::string_view path)
        {
            std::string lower {path};
            for(char &ch : lower)
            {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            return lower.ends_with(".wav") || lower.ends_with(".ogg") || lower.ends_with(".mp3") ||
                   lower.ends_with(".flac");
        }

        /// miniaudio keeps internal pointers to objects — addresses must stay stable.
        [[nodiscard]] std::string PathForMiniaudio(const std::filesystem::path &path)
        {
            const auto u8 = path.u8string();
            return std::string(u8.begin(), u8.end());
        }
    } // namespace

    struct MiniaudioEngine::InstanceEntry
    {
        std::unique_ptr<ma_sound> sound;
        bool                      soundReady = false;
        std::string               eventPath;
        EventDef                  def {};
        float                     userVolume = 1.0f;
        float                     userPitch  = 1.0f;
        bool                      spatial    = false;
    };

    MiniaudioEngine::MiniaudioEngine(const skr::Arc<skr::Logger<MiniaudioEngine>> &logger)
        : mLogger(logger)
    {
        mBusVolumes.emplace("bus:/", 1.0f);
        mBusVolumes.emplace("bus:/Master", 1.0f);
        mBusVolumes.emplace("bus:/SFX", 1.0f);
        mBusVolumes.emplace("bus:/Music", 1.0f);
    }

    MiniaudioEngine::~MiniaudioEngine()
    {
        Shutdown();
    }

    bool MiniaudioEngine::Initialize()
    {
        if(mInitialized)
        {
            return true;
        }

        auto *engine = new ma_engine {};
        const ma_result result = ma_engine_init(nullptr, engine);
        if(result != MA_SUCCESS)
        {
            if(mLogger)
            {
                mLogger->LogError("miniaudio ma_engine_init failed ({})", static_cast<int>(result));
            }
            delete engine;
            return false;
        }

        mEngine       = engine;
        mInitialized  = true;

        if(mLogger)
        {
            mLogger->LogInformation("miniaudio engine initialized");
        }
        return true;
    }

    void MiniaudioEngine::Update(float /*deltaTime*/)
    {
    }

    void MiniaudioEngine::Shutdown()
    {
        for(auto &[id, entry] : mInstances)
        {
            if(entry.soundReady && entry.sound)
            {
                ma_sound_uninit(entry.sound.get());
                entry.soundReady = false;
            }
            entry.sound.reset();
            (void)id;
        }
        mInstances.clear();

        if(mEngine != nullptr)
        {
            ma_engine_uninit(static_cast<ma_engine *>(mEngine));
            delete static_cast<ma_engine *>(mEngine);
            mEngine = nullptr;
        }

        mBankEvents.clear();
        mEvents.clear();
        mInitialized = false;
    }

    float MiniaudioEngine::BusGain(std::string_view busPath) const
    {
        const auto it = mBusVolumes.find(std::string(busPath));
        if(it == mBusVolumes.end())
        {
            return 1.0f;
        }
        const auto muteIt = mBusMuted.find(std::string(busPath));
        if(muteIt != mBusMuted.end() && muteIt->second)
        {
            return 0.0f;
        }
        const auto masterMute = mBusMuted.find("bus:/Master");
        if(masterMute != mBusMuted.end() && masterMute->second)
        {
            return 0.0f;
        }
        // Master fader is applied via ma_engine_set_volume — only return this bus's gain.
        return it->second;
    }

    void MiniaudioEngine::ApplyBusGain(InstanceEntry &entry) const
    {
        if(!entry.soundReady || !entry.sound)
        {
            return;
        }
        const float gain =
            entry.def.volume * entry.userVolume * BusGain(entry.def.bus);
        ma_sound_set_volume(entry.sound.get(), gain);
        ma_sound_set_pitch(entry.sound.get(), entry.def.pitch * entry.userPitch);
    }

    bool MiniaudioEngine::LoadBank(const std::filesystem::path &absolutePath,
                                   std::vector<std::string> &outEventPaths)
    {
        if(!Initialize())
        {
            return false;
        }

        const auto key = NormalizeBankKey(absolutePath);
        if(mBankEvents.contains(key))
        {
            outEventPaths = mBankEvents[key];
            return true;
        }

        std::ifstream file(absolutePath, std::ios::binary);
        if(!file)
        {
            if(mLogger)
            {
                mLogger->LogError("Failed to open audio bank '{}'", absolutePath.string());
            }
            return false;
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();

        AudioBankDto bank {};
        const simdjson::padded_string padded(buffer.str());
        if(const auto error = simdjson::from(padded).get(bank); error)
        {
            if(mLogger)
            {
                mLogger->LogError("Invalid audio bank JSON '{}': {}", absolutePath.string(),
                                  simdjson::error_message(error));
            }
            return false;
        }

        outEventPaths.clear();
        for(const auto &eventDto : bank.events)
        {
            if(eventDto.path.empty() || eventDto.clip.empty())
            {
                continue;
            }

            const auto clipAbsolute = AssetRegistry::ToAbsoluteResourcePath(eventDto.clip);
            if(!std::filesystem::is_regular_file(clipAbsolute))
            {
                if(mLogger)
                {
                    mLogger->LogWarning("Audio bank '{}' references missing clip '{}'",
                                        absolutePath.string(), eventDto.clip);
                }
                continue;
            }

            EventDef def {
                .clipAbsolute = clipAbsolute,
                .volume       = eventDto.volume.value_or(1.0f),
                .pitch        = eventDto.pitch.value_or(1.0f),
                .loop         = eventDto.loop.value_or(false),
                .bus          = eventDto.bus.value_or("bus:/SFX"),
            };
            mEvents[eventDto.path] = def;
            outEventPaths.push_back(eventDto.path);
        }

        mBankEvents.emplace(key, outEventPaths);

        if(mLogger)
        {
            mLogger->LogInformation("Loaded audio bank '{}' ({} events)", absolutePath.string(),
                                    outEventPaths.size());
        }
        return true;
    }

    bool MiniaudioEngine::UnloadBank(const std::filesystem::path &absolutePath)
    {
        const auto key = NormalizeBankKey(absolutePath);
        const auto it  = mBankEvents.find(key);
        if(it == mBankEvents.end())
        {
            return false;
        }

        for(const auto &eventPath : it->second)
        {
            mEvents.erase(eventPath);
        }
        mBankEvents.erase(it);
        return true;
    }

    std::vector<std::filesystem::path> MiniaudioEngine::GetLoadedBanks() const
    {
        std::vector<std::filesystem::path> banks;
        banks.reserve(mBankEvents.size());
        for(const auto &[path, events] : mBankEvents)
        {
            (void)events;
            banks.push_back(path);
        }
        return banks;
    }

    std::optional<MiniaudioEngine::EventDef> MiniaudioEngine::ResolveEvent(
        std::string_view eventPath) const
    {
        const auto it = mEvents.find(std::string(eventPath));
        if(it != mEvents.end())
        {
            return it->second;
        }

        if(IsDirectClipPath(eventPath))
        {
            std::filesystem::path clipPath {eventPath};
            if(!clipPath.is_absolute())
            {
                clipPath = AssetRegistry::ToAbsoluteResourcePath(clipPath);
            }
            if(std::filesystem::is_regular_file(clipPath))
            {
                return EventDef {.clipAbsolute = clipPath};
            }
        }

        return std::nullopt;
    }

    std::optional<AudioEventInfo> MiniaudioEngine::GetEventInfo(std::string_view eventPath) const
    {
        const auto def = ResolveEvent(eventPath);
        if(!def)
        {
            return std::nullopt;
        }

        AudioEventInfo info {.path = std::string(eventPath)};

        ma_decoder decoder {};
        const auto clipPath = PathForMiniaudio(def->clipAbsolute);
        if(ma_decoder_init_file(clipPath.c_str(), nullptr, &decoder) == MA_SUCCESS)
        {
            ma_uint64 lengthFrames = 0;
            if(ma_decoder_get_length_in_pcm_frames(&decoder, &lengthFrames) == MA_SUCCESS &&
               decoder.outputSampleRate > 0)
            {
                info.maxDurationSec =
                    static_cast<float>(lengthFrames) / static_cast<float>(decoder.outputSampleRate);
                info.minDurationSec = info.maxDurationSec;
            }
            ma_decoder_uninit(&decoder);
        }

        return info;
    }

    AudioEventInstance MiniaudioEngine::CreateEventInstance(std::string_view eventPath)
    {
        if(!Initialize())
        {
            return {};
        }

        const auto def = ResolveEvent(eventPath);
        if(!def)
        {
            if(mLogger)
            {
                mLogger->LogWarning("Unknown audio event '{}'", eventPath);
            }
            return {};
        }

        auto *engine = static_cast<ma_engine *>(mEngine);
        auto  sound  = std::make_unique<ma_sound>();

        const ma_uint32 flags = def->loop ? MA_SOUND_FLAG_LOOPING : 0;
        const auto      clipPath = PathForMiniaudio(def->clipAbsolute);
        if(ma_sound_init_from_file(engine, clipPath.c_str(), flags | MA_SOUND_FLAG_DECODE, nullptr,
                                   nullptr, sound.get()) != MA_SUCCESS)
        {
            if(mLogger)
            {
                mLogger->LogError("Failed to load clip '{}' for event '{}'",
                                  def->clipAbsolute.string(), eventPath);
            }
            return {};
        }

        // Heap-allocate the sound first so relocating InstanceEntry never moves ma_sound.
        const std::uint64_t id = mNextInstanceId++;
        InstanceEntry       entry {
                  .sound      = std::move(sound),
                  .soundReady = true,
                  .eventPath  = std::string(eventPath),
                  .def        = *def,
                  .spatial    = false,
        };
        ma_sound_set_spatialization_enabled(entry.sound.get(), MA_FALSE);
        ApplyBusGain(entry);

        mInstances.emplace(id, std::move(entry));
        return AudioEventInstance {.id = id};
    }

    MiniaudioEngine::InstanceEntry *MiniaudioEngine::TryGetInstance(AudioEventInstance instance)
    {
        const auto it = mInstances.find(instance.id);
        return it == mInstances.end() ? nullptr : &it->second;
    }

    const MiniaudioEngine::InstanceEntry *MiniaudioEngine::TryGetInstance(
        AudioEventInstance instance) const
    {
        const auto it = mInstances.find(instance.id);
        return it == mInstances.end() ? nullptr : &it->second;
    }

    void MiniaudioEngine::ReleaseEventInstance(AudioEventInstance instance)
    {
        const auto it = mInstances.find(instance.id);
        if(it == mInstances.end())
        {
            return;
        }
        if(it->second.soundReady && it->second.sound)
        {
            ma_sound_uninit(it->second.sound.get());
            it->second.soundReady = false;
        }
        it->second.sound.reset();
        mInstances.erase(it);
    }

    bool MiniaudioEngine::StartEvent(AudioEventInstance instance)
    {
        auto *entry = TryGetInstance(instance);
        if(entry == nullptr || !entry->soundReady || !entry->sound)
        {
            return false;
        }
        ApplyBusGain(*entry);
        (void)ma_sound_seek_to_pcm_frame(entry->sound.get(), 0);
        return ma_sound_start(entry->sound.get()) == MA_SUCCESS;
    }

    void MiniaudioEngine::StopEvent(AudioEventInstance instance, bool allowFadeOut)
    {
        auto *entry = TryGetInstance(instance);
        if(entry == nullptr || !entry->soundReady || !entry->sound)
        {
            return;
        }
        if(allowFadeOut)
        {
            ma_sound_set_fade_in_milliseconds(entry->sound.get(),
                                              ma_sound_get_volume(entry->sound.get()), 0.0f, 120);
        }
        ma_sound_stop(entry->sound.get());
    }

    void MiniaudioEngine::PauseEvent(AudioEventInstance instance, bool paused)
    {
        auto *entry = TryGetInstance(instance);
        if(entry == nullptr || !entry->soundReady || !entry->sound)
        {
            return;
        }
        if(paused)
        {
            ma_sound_stop(entry->sound.get());
        }
        else
        {
            (void)ma_sound_start(entry->sound.get());
        }
    }

    void MiniaudioEngine::SetEventVolume(AudioEventInstance instance, float volume)
    {
        auto *entry = TryGetInstance(instance);
        if(entry == nullptr)
        {
            return;
        }
        entry->userVolume = volume;
        ApplyBusGain(*entry);
    }

    void MiniaudioEngine::SetEventPitch(AudioEventInstance instance, float pitch)
    {
        auto *entry = TryGetInstance(instance);
        if(entry == nullptr)
        {
            return;
        }
        entry->userPitch = pitch;
        ApplyBusGain(*entry);
    }

    void MiniaudioEngine::SetEventLoop(AudioEventInstance instance, bool loop)
    {
        auto *entry = TryGetInstance(instance);
        if(entry == nullptr || !entry->soundReady || !entry->sound)
        {
            return;
        }
        entry->def.loop = loop;
        ma_sound_set_looping(entry->sound.get(), loop ? MA_TRUE : MA_FALSE);
    }

    void MiniaudioEngine::SetEventSpatialization(AudioEventInstance instance, bool enabled)
    {
        auto *entry = TryGetInstance(instance);
        if(entry == nullptr || !entry->soundReady || !entry->sound)
        {
            return;
        }
        entry->spatial = enabled;
        ma_sound_set_spatialization_enabled(entry->sound.get(), enabled ? MA_TRUE : MA_FALSE);
    }

    void MiniaudioEngine::SetEventMinMaxDistance(AudioEventInstance instance, float minDistance,
                                                 float maxDistance)
    {
        auto *entry = TryGetInstance(instance);
        if(entry == nullptr || !entry->soundReady || !entry->sound)
        {
            return;
        }
        const float minD = std::max(minDistance, 0.01f);
        const float maxD = std::max(maxDistance, minD);
        ma_sound_set_min_distance(entry->sound.get(), minD);
        ma_sound_set_max_distance(entry->sound.get(), maxD);
    }

    bool MiniaudioEngine::SetEventParameter(AudioEventInstance instance, std::string_view name,
                                            float value)
    {
        auto *entry = TryGetInstance(instance);
        if(entry == nullptr)
        {
            return false;
        }
        if(name == "volume")
        {
            SetEventVolume(instance, value);
            return true;
        }
        if(name == "pitch")
        {
            SetEventPitch(instance, value);
            return true;
        }
        return false;
    }

    void MiniaudioEngine::SetEvent3DAttributes(AudioEventInstance instance,
                                               const glm::vec3 &position,
                                               const glm::vec3 & /*velocity*/)
    {
        auto *entry = TryGetInstance(instance);
        if(entry == nullptr || !entry->soundReady || !entry->sound)
        {
            return;
        }
        ma_sound_set_position(entry->sound.get(), position.x, position.y, position.z);
    }

    bool MiniaudioEngine::IsEventPlaying(AudioEventInstance instance) const
    {
        const auto *entry = TryGetInstance(instance);
        if(entry == nullptr || !entry->soundReady || !entry->sound)
        {
            return false;
        }
        return ma_sound_is_playing(entry->sound.get()) == MA_TRUE;
    }

    bool MiniaudioEngine::IsEventAtEnd(AudioEventInstance instance) const
    {
        const auto *entry = TryGetInstance(instance);
        if(entry == nullptr || !entry->soundReady || !entry->sound)
        {
            return true;
        }
        if(ma_sound_is_playing(entry->sound.get()) == MA_TRUE)
        {
            return false;
        }

        ma_uint64 cursor = 0;
        ma_uint64 length = 0;
        (void)ma_sound_get_cursor_in_pcm_frames(entry->sound.get(), &cursor);
        (void)ma_sound_get_length_in_pcm_frames(entry->sound.get(), &length);
        if(length == 0)
        {
            // Unknown length: treat a previously started, now-stopped sound as finished.
            return true;
        }
        return cursor + 1 >= length;
    }

    void MiniaudioEngine::SetListenerTransform(const glm::vec3 &position,
                                               const glm::quat &rotation)
    {
        if(!mInitialized || mEngine == nullptr)
        {
            return;
        }

        auto *engine = static_cast<ma_engine *>(mEngine);
        const glm::vec3 forward = glm::normalize(ForwardFromQuat(rotation));
        const glm::vec3 up      = glm::normalize(UpFromQuat(rotation));

        ma_engine_listener_set_position(engine, 0, position.x, position.y, position.z);
        ma_engine_listener_set_direction(engine, 0, forward.x, forward.y, forward.z);
        ma_engine_listener_set_world_up(engine, 0, up.x, up.y, up.z);
    }

    std::vector<AudioBusInfo> MiniaudioEngine::GetMixerBuses() const
    {
        std::vector<AudioBusInfo> buses;
        for(const auto &[path, volume] : mBusVolumes)
        {
            const auto muteIt = mBusMuted.find(path);
            buses.push_back(AudioBusInfo {
                .path   = path,
                .volume = volume,
                .muted  = muteIt != mBusMuted.end() && muteIt->second,
            });
        }
        return buses;
    }

    std::vector<AudioVcaInfo> MiniaudioEngine::GetMixerVcas() const
    {
        return {};
    }

    void MiniaudioEngine::SetBusVolume(std::string_view busPath, float volume)
    {
        mBusVolumes[std::string(busPath)] = volume;

        for(auto &[id, entry] : mInstances)
        {
            if(entry.def.bus == busPath || busPath == "bus:/Master" || busPath == "bus:/")
            {
                ApplyBusGain(entry);
            }
            (void)id;
        }

        if(busPath == "bus:/Master" || busPath == "bus:/")
        {
            if(mEngine != nullptr)
            {
                ma_engine_set_volume(static_cast<ma_engine *>(mEngine), volume);
            }
        }
    }

    void MiniaudioEngine::SetBusMute(std::string_view busPath, bool muted)
    {
        mBusMuted[std::string(busPath)] = muted;
        for(auto &[id, entry] : mInstances)
        {
            if(entry.def.bus == busPath || busPath == "bus:/Master" || busPath == "bus:/")
            {
                ApplyBusGain(entry);
            }
            (void)id;
        }
    }

    void MiniaudioEngine::SetVcaVolume(std::string_view /*vcaPath*/, float /*volume*/)
    {
    }

    std::optional<WaveformData> MiniaudioEngine::DecodeWaveform(
        const std::filesystem::path &absolutePath, int targetPeakCount) const
    {
        if(targetPeakCount <= 0)
        {
            return std::nullopt;
        }

        ma_decoder decoder {};
        ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
        const auto        clipPath = PathForMiniaudio(absolutePath);
        if(ma_decoder_init_file(clipPath.c_str(), &config, &decoder) != MA_SUCCESS)
        {
            return std::nullopt;
        }

        WaveformData data {
            .sampleRate = static_cast<int>(decoder.outputSampleRate),
            .channels   = static_cast<int>(decoder.outputChannels),
        };

        ma_uint64 totalFrames = 0;
        (void)ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames);
        if(decoder.outputSampleRate > 0)
        {
            data.durationSec =
                static_cast<float>(totalFrames) / static_cast<float>(decoder.outputSampleRate);
        }

        const ma_uint64 bucketFrames =
            std::max<ma_uint64>(1, totalFrames / static_cast<ma_uint64>(targetPeakCount));
        std::vector<float> frameBuffer(static_cast<std::size_t>(decoder.outputChannels));

        (void)ma_decoder_seek_to_pcm_frame(&decoder, 0);
        data.peaks.reserve(static_cast<std::size_t>(targetPeakCount));

        for(int bucket = 0; bucket < targetPeakCount; ++bucket)
        {
            float peak = 0.0f;
            for(ma_uint64 f = 0; f < bucketFrames; ++f)
            {
                ma_uint64 framesRead = 0;
                if(ma_decoder_read_pcm_frames(&decoder, frameBuffer.data(), 1, &framesRead) !=
                       MA_SUCCESS ||
                   framesRead == 0)
                {
                    break;
                }
                for(ma_uint32 ch = 0; ch < decoder.outputChannels; ++ch)
                {
                    peak = std::max(peak, std::abs(frameBuffer[ch]));
                }
            }
            data.peaks.push_back(peak);
        }

        ma_decoder_uninit(&decoder);
        return data;
    }

} // namespace FRIGGA_NAMESPACE
