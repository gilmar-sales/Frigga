#include "AudioSystem.hpp"

#include "Frigga/ECS/Components/AudioSourceComponent.hpp"
#include "Frigga/ECS/Components/HierarchyComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"
#include "Frigga/ECS/TransformUtil.hpp"

namespace FRIGGA_NAMESPACE
{

    AudioSystem::AudioSystem(const skr::Arc<fr::Registry> &registry,
                             const skr::Arc<IAudioEngine> &audioEngine,
                             const skr::Arc<SceneSimulationState> &simulation,
                             const skr::Arc<Scene> &scene,
                             const skr::Arc<AudioController> &controller)
        : System(registry), mAudioEngine(audioEngine), mSimulation(simulation), mScene(scene),
          mController(controller)
    {
        if(!mAudioEngine->IsInitialized())
        {
            (void)mAudioEngine->Initialize();
        }
    }

    void AudioSystem::releaseSource(AudioSourceComponent &source)
    {
        if(source.instance.IsValid())
        {
            mAudioEngine->StopEvent(source.instance, true);
            mAudioEngine->ReleaseEventInstance(source.instance);
            source.instance = {};
        }
        source.engineStarted = false;
    }

    void AudioSystem::stopAllSources()
    {
        if(mController)
        {
            mController->StopAllPlayback();
        }
    }

    void AudioSystem::applySourceProperties(AudioSourceComponent &source)
    {
        if(!source.instance.IsValid())
        {
            return;
        }

        mAudioEngine->SetEventVolume(source.instance, source.volume);
        mAudioEngine->SetEventPitch(source.instance, source.pitch);
        mAudioEngine->SetEventLoop(source.instance, source.loop);
        mAudioEngine->SetEventSpatialization(source.instance, source.is3D);
        if(source.is3D)
        {
            mAudioEngine->SetEventMinMaxDistance(source.instance, source.minDistance,
                                                 source.maxDistance);
        }

        for(const auto &[name, value] : source.parameters)
        {
            (void)mAudioEngine->SetEventParameter(source.instance, name, value);
        }
    }

    void AudioSystem::syncListener()
    {
        fr::Entity listenerEntity = kInvalidEntity;
        mRegistry->CreateMutation()->Each(
            [&](fr::Entity entity, AudioListenerComponent &listener, TransformComponent &) {
                if(listener.active && listenerEntity == kInvalidEntity)
                {
                    listenerEntity = entity;
                }
            });

        if(listenerEntity == kInvalidEntity && mScene)
        {
            listenerEntity = mScene->GetMainCameraEntity();
            if(listenerEntity != kInvalidEntity &&
               !mRegistry->HasComponent<TransformComponent>(listenerEntity))
            {
                listenerEntity = kInvalidEntity;
            }
        }

        if(listenerEntity == kInvalidEntity)
        {
            return;
        }

        const auto pose = TransformUtil::WorldPose(*mRegistry, listenerEntity);
        mAudioEngine->SetListenerTransform(pose.position, pose.rotation);
    }

    void AudioSystem::syncSources()
    {
        mRegistry->CreateMutation()->Each(
            [&](fr::Entity entity, AudioSourceComponent &source, TransformComponent &) {
                if(source.eventPath.empty())
                {
                    releaseSource(source);
                    return;
                }

                if(source.playOnAwake && !source.awakeApplied &&
                   source.desired == AudioPlaybackState::Stopped)
                {
                    source.desired      = AudioPlaybackState::Playing;
                    source.awakeApplied = true;
                }

                if(source.desired == AudioPlaybackState::Stopped)
                {
                    releaseSource(source);
                    return;
                }

                if(!source.instance.IsValid())
                {
                    source.instance = mAudioEngine->CreateEventInstance(source.eventPath);
                    if(!source.instance.IsValid())
                    {
                        source.desired = AudioPlaybackState::Stopped;
                        source.oneShot = false;
                        return;
                    }
                    applySourceProperties(source);
                    if(source.desired == AudioPlaybackState::Playing)
                    {
                        if(!mAudioEngine->StartEvent(source.instance))
                        {
                            releaseSource(source);
                            source.desired       = AudioPlaybackState::Stopped;
                            source.oneShot       = false;
                            source.engineStarted = false;
                            return;
                        }
                        source.engineStarted = true;
                    }
                    else if(source.desired == AudioPlaybackState::Paused)
                    {
                        if(mAudioEngine->StartEvent(source.instance))
                        {
                            source.engineStarted = true;
                            mAudioEngine->PauseEvent(source.instance, true);
                        }
                    }
                }
                else
                {
                    applySourceProperties(source);

                    const bool enginePlaying = mAudioEngine->IsEventPlaying(source.instance);
                    if(source.desired == AudioPlaybackState::Playing)
                    {
                        if(!enginePlaying)
                        {
                            // Non-looping clip finished: stop. Looping: restart from start.
                            // Mid-clip stop (pause) is not at end — StartEvent resumes/restarts.
                            if(source.engineStarted && !source.loop &&
                               mAudioEngine->IsEventAtEnd(source.instance))
                            {
                                releaseSource(source);
                                source.desired       = AudioPlaybackState::Stopped;
                                source.oneShot       = false;
                                source.engineStarted = false;
                                return;
                            }
                            if(mAudioEngine->StartEvent(source.instance))
                            {
                                source.engineStarted = true;
                            }
                        }
                        else
                        {
                            source.engineStarted = true;
                        }
                    }
                    else if(source.desired == AudioPlaybackState::Paused)
                    {
                        mAudioEngine->PauseEvent(source.instance, true);
                    }
                }

                if(source.instance.IsValid() && source.is3D)
                {
                    const auto pose = TransformUtil::WorldPose(*mRegistry, entity);
                    mAudioEngine->SetEvent3DAttributes(source.instance, pose.position,
                                                       glm::vec3(0.0f));
                }
            });
    }

    void AudioSystem::Update(float deltaTime)
    {
        const bool playing = mSimulation->IsPlaying();

        if(!mWasPlaying && playing)
        {
            if(mController)
            {
                mController->StopPreview();
            }
        }

        if(mWasPlaying && !playing)
        {
            stopAllSources();
        }

        mWasPlaying = playing;

        if(!playing)
        {
            return;
        }

        if(!mSimulation->IsRunning())
        {
            // Simulation paused: keep instances but pause playback.
            mRegistry->CreateMutation()->Each(
                [&](fr::Entity /*entity*/, AudioSourceComponent &source) {
                    if(source.instance.IsValid())
                    {
                        mAudioEngine->PauseEvent(source.instance, true);
                    }
                });
            return;
        }

        mAudioEngine->Update(deltaTime);
        syncListener();
        syncSources();
    }

} // namespace FRIGGA_NAMESPACE
