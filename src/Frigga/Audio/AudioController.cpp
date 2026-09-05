#include <Frigga/Audio/AudioController.hpp>

namespace FRIGGA_NAMESPACE
{

    AudioController::AudioController(const skr::Arc<fr::Registry> &registry,
                                     const skr::Arc<IAudioEngine> &audioEngine)
        : mRegistry(registry), mAudioEngine(audioEngine)
    {
        if(!mAudioEngine->IsInitialized())
        {
            (void)mAudioEngine->Initialize();
        }
    }

    bool AudioController::Play(fr::Entity entity, std::string_view eventPath, bool oneShot)
    {
        if(!mRegistry->HasComponent<AudioSourceComponent>(entity))
        {
            return false;
        }

        bool ok = false;
        mRegistry->TryGetComponents<AudioSourceComponent>(
            entity, [&](AudioSourceComponent &source) {
                if(!eventPath.empty())
                {
                    source.eventPath = std::string(eventPath);
                }
                if(source.eventPath.empty())
                {
                    return;
                }

                source.desired = AudioPlaybackState::Playing;
                source.oneShot = oneShot;
                ok             = true;
            });
        return ok;
    }

    void AudioController::Stop(fr::Entity entity)
    {
        mRegistry->TryGetComponents<AudioSourceComponent>(
            entity, [](AudioSourceComponent &source) {
                source.desired = AudioPlaybackState::Stopped;
                source.oneShot = false;
            });
    }

    void AudioController::Pause(fr::Entity entity)
    {
        mRegistry->TryGetComponents<AudioSourceComponent>(
            entity, [](AudioSourceComponent &source) {
                if(source.desired == AudioPlaybackState::Playing)
                {
                    source.desired = AudioPlaybackState::Paused;
                }
            });
    }

    void AudioController::Resume(fr::Entity entity)
    {
        mRegistry->TryGetComponents<AudioSourceComponent>(
            entity, [](AudioSourceComponent &source) {
                if(source.desired == AudioPlaybackState::Paused)
                {
                    source.desired = AudioPlaybackState::Playing;
                }
            });
    }

    void AudioController::SetVolume(fr::Entity entity, float volume)
    {
        mRegistry->TryGetComponents<AudioSourceComponent>(
            entity, [volume](AudioSourceComponent &source) { source.volume = volume; });
    }

    void AudioController::SetPitch(fr::Entity entity, float pitch)
    {
        mRegistry->TryGetComponents<AudioSourceComponent>(
            entity, [pitch](AudioSourceComponent &source) { source.pitch = pitch; });
    }

    bool AudioController::SetParameter(fr::Entity entity, std::string_view name, float value)
    {
        if(name.empty())
        {
            return false;
        }

        bool ok = false;
        mRegistry->TryGetComponents<AudioSourceComponent>(
            entity, [&](AudioSourceComponent &source) {
                source.parameters[std::string(name)] = value;
                ok                                   = true;
            });
        return ok;
    }

    void AudioController::SetGlobalParameter(std::string_view /*name*/, float /*value*/)
    {
        // Global routing is mixer-bus driven for miniaudio; reserved for future banks.
    }

    bool AudioController::IsPlaying(fr::Entity entity) const
    {
        bool playing = false;
        mRegistry->TryGetComponents<AudioSourceComponent>(
            entity, [&](AudioSourceComponent &source) {
                playing = source.desired == AudioPlaybackState::Playing;
            });
        return playing;
    }

    bool AudioController::PreviewEvent(std::string_view eventPath, float volume, bool loop)
    {
        if(eventPath.empty())
        {
            return false;
        }

        StopPreview();

        mPreview.instance = mAudioEngine->CreateEventInstance(eventPath);
        if(!mPreview.instance.IsValid())
        {
            return false;
        }

        mAudioEngine->SetEventVolume(mPreview.instance, volume);
        mAudioEngine->SetEventLoop(mPreview.instance, loop);
        mAudioEngine->SetEventSpatialization(mPreview.instance, false);
        return mAudioEngine->StartEvent(mPreview.instance);
    }

    void AudioController::StopPreview()
    {
        if(!mPreview.instance.IsValid())
        {
            return;
        }
        mAudioEngine->StopEvent(mPreview.instance, true);
        mAudioEngine->ReleaseEventInstance(mPreview.instance);
        mPreview.instance = {};
    }

    bool AudioController::PlayOneShotAt(std::string_view eventPath, const glm::vec3 &position,
                                        float volume)
    {
        if(eventPath.empty())
        {
            return false;
        }

        AudioEventInstance instance = mAudioEngine->CreateEventInstance(eventPath);
        if(!instance.IsValid())
        {
            return false;
        }

        mAudioEngine->SetEventVolume(instance, volume);
        mAudioEngine->SetEventLoop(instance, false);
        mAudioEngine->SetEventSpatialization(instance, true);
        mAudioEngine->SetEvent3DAttributes(instance, position, glm::vec3(0.0f));
        if(!mAudioEngine->StartEvent(instance))
        {
            mAudioEngine->ReleaseEventInstance(instance);
            return false;
        }

        mOneShots.push_back(OneShotVoice {.instance = instance, .position = position});
        return true;
    }

    void AudioController::UpdateOneShots()
    {
        std::erase_if(mOneShots, [this](OneShotVoice &voice) {
            if(!voice.instance.IsValid())
            {
                return true;
            }

            if(mAudioEngine->IsEventPlaying(voice.instance))
            {
                return false;
            }

            mAudioEngine->StopEvent(voice.instance, true);
            mAudioEngine->ReleaseEventInstance(voice.instance);
            return true;
        });
    }

    void AudioController::StopAllPlayback()
    {
        StopPreview();

        for(auto &voice : mOneShots)
        {
            if(voice.instance.IsValid())
            {
                mAudioEngine->StopEvent(voice.instance, true);
                mAudioEngine->ReleaseEventInstance(voice.instance);
            }
        }
        mOneShots.clear();

        mRegistry->CreateMutation()->Each(
            [&](fr::Entity /*entity*/, AudioSourceComponent &source) {
                if(source.instance.IsValid())
                {
                    mAudioEngine->StopEvent(source.instance, true);
                    mAudioEngine->ReleaseEventInstance(source.instance);
                    source.instance = {};
                }
                source.desired       = AudioPlaybackState::Stopped;
                source.oneShot       = false;
                source.awakeApplied  = false;
                source.engineStarted = false;
            });
    }

} // namespace FRIGGA_NAMESPACE
