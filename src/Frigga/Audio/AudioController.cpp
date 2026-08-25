#include "AudioController.hpp"

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

        bool started = false;
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

                source.userPlaying = !oneShot;
                source.playOnAwake = oneShot;

                if(!source.instance.IsValid())
                {
                    source.instance = mAudioEngine->CreateEventInstance(source.eventPath);
                }
                if(source.instance.IsValid())
                {
                    mAudioEngine->SetEventVolume(source.instance, source.volume);
                    mAudioEngine->SetEventPitch(source.instance, source.pitch);
                    started = mAudioEngine->StartEvent(source.instance);
                    source.started = started;
                }
            });
        return started;
    }

    void AudioController::Stop(fr::Entity entity)
    {
        mRegistry->TryGetComponents<AudioSourceComponent>(
            entity, [&](AudioSourceComponent &source) {
                source.userPlaying = false;
                source.playOnAwake = false;
                if(source.instance.IsValid())
                {
                    mAudioEngine->StopEvent(source.instance, true);
                    mAudioEngine->ReleaseEventInstance(source.instance);
                    source.instance = {};
                }
                source.started = false;
            });
    }

    void AudioController::Pause(fr::Entity entity)
    {
        mRegistry->TryGetComponents<AudioSourceComponent>(
            entity, [&](AudioSourceComponent &source) {
                if(source.instance.IsValid())
                {
                    mAudioEngine->PauseEvent(source.instance, true);
                }
            });
    }

    void AudioController::Resume(fr::Entity entity)
    {
        mRegistry->TryGetComponents<AudioSourceComponent>(
            entity, [&](AudioSourceComponent &source) {
                if(source.instance.IsValid())
                {
                    mAudioEngine->PauseEvent(source.instance, false);
                }
            });
    }

    void AudioController::SetVolume(fr::Entity entity, float volume)
    {
        mRegistry->TryGetComponents<AudioSourceComponent>(
            entity, [&](AudioSourceComponent &source) {
                source.volume = volume;
                if(source.instance.IsValid())
                {
                    mAudioEngine->SetEventVolume(source.instance, volume);
                }
            });
    }

    void AudioController::SetPitch(fr::Entity entity, float pitch)
    {
        mRegistry->TryGetComponents<AudioSourceComponent>(
            entity, [&](AudioSourceComponent &source) {
                source.pitch = pitch;
                if(source.instance.IsValid())
                {
                    mAudioEngine->SetEventPitch(source.instance, pitch);
                }
            });
    }

    bool AudioController::SetParameter(fr::Entity entity, std::string_view name, float value)
    {
        bool ok = false;
        mRegistry->TryGetComponents<AudioSourceComponent>(
            entity, [&](AudioSourceComponent &source) {
                if(source.instance.IsValid())
                {
                    ok = mAudioEngine->SetEventParameter(source.instance, name, value);
                }
            });
        return ok;
    }

    void AudioController::SetGlobalParameter(std::string_view name, float value)
    {
        if(!mAudioEngine->IsInitialized())
        {
            return;
        }
        // Global parameters are not wired yet for miniaudio event banks.
        // For now, callers should use per-entity parameters; global routing can be extended later.
        (void)name;
        (void)value;
    }

    bool AudioController::IsPlaying(fr::Entity entity) const
    {
        bool playing = false;
        mRegistry->TryGetComponents<AudioSourceComponent>(
            entity, [&](AudioSourceComponent &source) {
                if(source.instance.IsValid())
                {
                    playing = mAudioEngine->IsEventPlaying(source.instance);
                }
            });
        return playing;
    }

    bool AudioController::PreviewEvent(std::string_view eventPath, float volume)
    {
        if(eventPath.empty())
        {
            return false;
        }

        if(mPreview.instance.IsValid())
        {
            mAudioEngine->StopEvent(mPreview.instance, true);
            mAudioEngine->ReleaseEventInstance(mPreview.instance);
            mPreview.instance = {};
        }

        mPreview.instance = mAudioEngine->CreateEventInstance(eventPath);
        if(!mPreview.instance.IsValid())
        {
            return false;
        }

        mAudioEngine->SetEventVolume(mPreview.instance, volume);
        return mAudioEngine->StartEvent(mPreview.instance);
    }

} // namespace FRIGGA_NAMESPACE
