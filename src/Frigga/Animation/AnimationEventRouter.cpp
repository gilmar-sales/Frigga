#include "AnimationEventRouter.hpp"

#include "Frigga/ECS/Components/AudioSourceComponent.hpp"

#include <algorithm>
#include <cctype>

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        [[nodiscard]] bool IsFootstepEvent(std::string_view name)
        {
            return name.find("Footstep") != std::string_view::npos;
        }
    } // namespace

    AnimationEventRouter::AnimationEventRouter(const skr::Arc<fr::Registry> &registry,
                                               const skr::Arc<AudioController> &audio,
                                               const skr::Arc<SceneSimulationState> &simulation)
        : mRegistry(registry), mAudio(audio), mSimulation(simulation)
    {
    }

    void AnimationEventRouter::SetGameplayHandler(GameplayHandler handler)
    {
        mGameplayHandler = std::move(handler);
    }

    std::string AnimationEventRouter::resolveAudioPath(const AnimatorComponent &animator,
                                                       std::string_view eventName) const
    {
        for(const auto &[marker, path] : animator.clipEventRoutes)
        {
            if(marker == eventName)
            {
                return path;
            }
        }

        if(IsFootstepEvent(eventName) && !animator.footstepEventPath.empty())
        {
            return animator.footstepEventPath;
        }

        return std::string(eventName);
    }

    void AnimationEventRouter::Dispatch(fr::Entity entity, const AnimatorComponent &animator,
                                        std::span<const fra::FiredAnimationEvent> events)
    {
        if(events.empty() || !animator.routeClipEvents)
        {
            return;
        }

        if(mGameplayHandler)
        {
            for(const auto &event : events)
            {
                mGameplayHandler(entity, event);
            }
        }

        if(!mSimulation || !mSimulation->IsRunning() || !mAudio)
        {
            return;
        }

        const auto pose = TransformUtil::WorldPose(*mRegistry, entity);

        for(const auto &event : events)
        {
            const std::string audioPath = resolveAudioPath(animator, event.name);
            if(audioPath.empty())
            {
                continue;
            }

            if(mRegistry->HasComponent<AudioSourceComponent>(entity))
            {
                (void)mAudio->Play(entity, audioPath, true);
            }
            else
            {
                (void)mAudio->PlayOneShotAt(audioPath, pose.position, 1.0f);
            }
        }
    }

} // namespace FRIGGA_NAMESPACE
