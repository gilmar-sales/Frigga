#include <Frigga/ECS/Systems/AudioSystem.hpp>

#include "Frigga/ECS/Components/AudioSourceComponent.hpp"
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

    void AudioSystem::releaseSource(fr::Entity entity, AudioSourceComponent &source)
    {
        if(source.instance.IsValid())
        {
            mAudioEngine->StopEvent(source.instance, true);
            mAudioEngine->ReleaseEventInstance(source.instance);
            source.instance = {};
        }
        source.engineStarted = false;
        mAppliedProps.erase(entity);
    }

    void AudioSystem::stopAllSources()
    {
        if(mController)
        {
            mController->StopAllPlayback();
        }
    }

    void AudioSystem::applySourceProperties(fr::Entity entity, AudioSourceComponent &source)
    {
        if(!source.instance.IsValid())
        {
            return;
        }

        // New (or re-created) instance: the engine holds bank defaults, so push
        // every property unconditionally. Otherwise only push what changed —
        // miniaudio Set* calls every frame are pure overhead in the steady state.
        const auto it = mAppliedProps.find(entity);
        const bool fullPush =
            it == mAppliedProps.end() || it->second.instanceId != source.instance.id;
        const AppliedSourceProps prev = fullPush ? AppliedSourceProps {} : it->second;

        AppliedSourceProps next = prev;
        next.instanceId          = source.instance.id;
        if(fullPush || prev.volume != source.volume)
        {
            mAudioEngine->SetEventVolume(source.instance, source.volume);
            next.volume = source.volume;
        }
        if(fullPush || prev.pitch != source.pitch)
        {
            mAudioEngine->SetEventPitch(source.instance, source.pitch);
            next.pitch = source.pitch;
        }
        if(fullPush || prev.loop != source.loop)
        {
            mAudioEngine->SetEventLoop(source.instance, source.loop);
            next.loop = source.loop;
        }
        if(fullPush || prev.is3D != source.is3D)
        {
            mAudioEngine->SetEventSpatialization(source.instance, source.is3D);
            next.is3D = source.is3D;
        }
        if(source.is3D && (fullPush || prev.minDistance != source.minDistance ||
                           prev.maxDistance != source.maxDistance))
        {
            mAudioEngine->SetEventMinMaxDistance(source.instance, source.minDistance,
                                                 source.maxDistance);
            next.minDistance = source.minDistance;
            next.maxDistance = source.maxDistance;
        }

        for(const auto &[name, value] : source.parameters)
        {
            const auto paramIt = prev.parameters.find(name);
            if(fullPush || paramIt == prev.parameters.end() || paramIt->second != value)
            {
                (void)mAudioEngine->SetEventParameter(source.instance, name, value);
            }
        }
        next.parameters = source.parameters;
        mAppliedProps[entity] = std::move(next);
    }

    void AudioSystem::syncListener()
    {
        fr::Entity listenerEntity = fr::NullEntity;
        mRegistry->CreateMutation()->Each(
            [&](fr::Entity entity, AudioListenerComponent &listener, TransformComponent &) {
                if(listener.active && listenerEntity == fr::NullEntity)
                {
                    listenerEntity = entity;
                }
            });

        if(listenerEntity == fr::NullEntity && mScene)
        {
            listenerEntity = mScene->GetMainCameraEntity();
            if(listenerEntity != fr::NullEntity &&
               !mRegistry->HasComponent<TransformComponent>(listenerEntity))
            {
                listenerEntity = fr::NullEntity;
            }
        }

        if(listenerEntity == fr::NullEntity)
        {
            return;
        }

        const auto pose = TransformUtil::GetWorldPose(*mRegistry, listenerEntity);
        mAudioEngine->SetListenerTransform(pose.position, pose.rotation);
    }

    void AudioSystem::syncSources()
    {
        mRegistry->CreateMutation()->Each(
            [&](fr::Entity entity, AudioSourceComponent &source, TransformComponent &) {
                if(source.eventPath.empty())
                {
                    releaseSource(entity, source);
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
                    releaseSource(entity, source);
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
                    applySourceProperties(entity, source);
                    if(source.desired == AudioPlaybackState::Playing)
                    {
                        if(!mAudioEngine->StartEvent(source.instance))
                        {
                            releaseSource(entity, source);
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
                    applySourceProperties(entity, source);

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
                                releaseSource(entity, source);
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
                    const auto pose = TransformUtil::GetWorldPose(*mRegistry, entity);
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
        if(mController)
        {
            mController->UpdateOneShots();
        }
        syncListener();
        syncSources();
    }

} // namespace FRIGGA_NAMESPACE
