#include "AudioSystem.hpp"

#include "Frigga/ECS/Components/AudioSourceComponent.hpp"
#include "Frigga/ECS/Components/HierarchyComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"
#include "Frigga/ECS/TransformUtil.hpp"

namespace FRIGGA_NAMESPACE
{

    AudioSystem::AudioSystem(const skr::Arc<fr::Registry> &registry,
                             const skr::Arc<IAudioEngine> &audioEngine,
                             const skr::Arc<SceneSimulationState> &simulation)
        : System(registry), mAudioEngine(audioEngine), mSimulation(simulation)
    {
        if(!mAudioEngine->IsInitialized())
        {
            (void)mAudioEngine->Initialize();
        }
    }

    void AudioSystem::stopAllSources()
    {
        mRegistry->CreateMutation()->Each<AudioSourceComponent>(
            [&](auto /*entity*/, AudioSourceComponent &source) {
                if(source.instance.IsValid())
                {
                    mAudioEngine->StopEvent(source.instance, true);
                    mAudioEngine->ReleaseEventInstance(source.instance);
                    source.instance = {};
                }
                source.started     = false;
                source.userPlaying = false;
            });
    }

    void AudioSystem::syncListener()
    {
        fr::Entity listenerEntity = kInvalidEntity;
        mRegistry->CreateMutation()->Each<AudioListenerComponent, TransformComponent>(
            [&](auto entity, AudioListenerComponent &listener, TransformComponent &) {
                if(listener.active && listenerEntity == kInvalidEntity)
                {
                    listenerEntity = entity;
                }
            });

        if(listenerEntity == kInvalidEntity)
        {
            return;
        }

        const auto pose = TransformUtil::WorldPose(*mRegistry, listenerEntity);
        mAudioEngine->SetListenerTransform(pose.position, pose.rotation);
    }

    void AudioSystem::syncSources(float /*deltaTime*/)
    {
        mRegistry->CreateMutation()->Each<AudioSourceComponent, TransformComponent>(
            [&](auto entity, AudioSourceComponent &source, TransformComponent &) {
                if(source.eventPath.empty())
                {
                    return;
                }

                const bool shouldPlay =
                    source.userPlaying || (source.playOnAwake && !source.started);

                if(!source.instance.IsValid() && shouldPlay)
                {
                    source.instance = mAudioEngine->CreateEventInstance(source.eventPath);
                    if(source.instance.IsValid())
                    {
                        mAudioEngine->SetEventVolume(source.instance, source.volume);
                        mAudioEngine->SetEventPitch(source.instance, source.pitch);
                        (void)mAudioEngine->StartEvent(source.instance);
                        source.started = true;
                    }
                }
                else if(source.instance.IsValid())
                {
                    mAudioEngine->SetEventVolume(source.instance, source.volume);
                    mAudioEngine->SetEventPitch(source.instance, source.pitch);

                    if(shouldPlay && !mAudioEngine->IsEventPlaying(source.instance))
                    {
                        (void)mAudioEngine->StartEvent(source.instance);
                    }
                    else if(!shouldPlay && mAudioEngine->IsEventPlaying(source.instance))
                    {
                        mAudioEngine->StopEvent(source.instance, true);
                    }
                }

                if(source.instance.IsValid() && source.is3D)
                {
                    const auto pose = TransformUtil::WorldPose(*mRegistry, entity);
                    mAudioEngine->SetEvent3DAttributes(source.instance, pose.position,
                                                       glm::vec3(0.0f));
                }

                (void)entity;
            });
    }

    void AudioSystem::Update(float deltaTime)
    {
        const bool playing = mSimulation->IsPlaying();

        if(mWasPlaying && !playing)
        {
            stopAllSources();
        }

        mWasPlaying = playing;

        if(!playing)
        {
            return;
        }

        mAudioEngine->Update(deltaTime);
        syncListener();
        syncSources(deltaTime);
    }

} // namespace FRIGGA_NAMESPACE
