#pragma once

#include "Frigga/Asset/AssetRegistry.hpp"

#include <Freyr/Freyr.hpp>

#include <string>
#include <string_view>
#include <unordered_map>

namespace FRIGGA_NAMESPACE
{

    /**
     * @brief Gameplay-facing animator API (Play / CrossFade / params).
     *
     * Mutates AnimatorComponent on entities and keeps per-entity playback
     * runtime (cross-fade + float/bool/trigger params) for AnimationSystem.
     *
     * Typical use during Play:
     * @code
     * controller->Play(fox, "Run");
     * controller->CrossFade(fox, "Idle", 0.2f);
     * controller->SetFloat(fox, "Speed", 1.4f);
     * controller->SetTrigger(fox, "Attack");
     * @endcode
     */
    class AnimationController
    {
      public:
        struct EntityRuntime
        {
            std::unordered_map<std::string, float> floats;
            std::unordered_map<std::string, bool>  bools;
            std::unordered_map<std::string, bool>  triggers;

            std::string fromClip;
            float       fromTimeSec        = 0.0f;
            bool        crossFading        = false;
            float       crossFadeDuration  = 0.0f;
            float       crossFadeElapsed   = 0.0f;
        };

        AnimationController(const skr::Arc<fr::Registry> &registry,
                            const skr::Arc<AssetRegistry> &assets);

        /// Snap or cross-fade to a clip by name (substring match allowed).
        bool Play(fr::Entity entity, std::string_view clipName, float crossFadeSeconds = 0.0f);

        /// Cross-fade to clip (convenience for Play with duration > 0).
        bool CrossFade(fr::Entity entity, std::string_view clipName,
                       float durationSeconds = 0.2f);

        void Stop(fr::Entity entity);
        void Pause(fr::Entity entity);
        void Resume(fr::Entity entity);

        void SetSpeed(fr::Entity entity, float speed);
        void SetLoop(fr::Entity entity, bool loop);
        void SetTime(fr::Entity entity, float timeSec);

        void SetFloat(fr::Entity entity, std::string_view name, float value);
        void SetBool(fr::Entity entity, std::string_view name, bool value);
        void SetTrigger(fr::Entity entity, std::string_view name);

        [[nodiscard]] float GetFloat(fr::Entity entity, std::string_view name,
                                     float fallback = 0.0f) const;
        [[nodiscard]] bool GetBool(fr::Entity entity, std::string_view name,
                                   bool fallback = false) const;
        [[nodiscard]] bool ConsumeTrigger(fr::Entity entity, std::string_view name);

        [[nodiscard]] std::string_view GetState(fr::Entity entity) const;
        [[nodiscard]] bool IsPlaying(fr::Entity entity) const;
        [[nodiscard]] bool IsCrossFading(fr::Entity entity) const;

        [[nodiscard]] EntityRuntime *TryGetRuntime(fr::Entity entity);
        [[nodiscard]] const EntityRuntime *TryGetRuntime(fr::Entity entity) const;
        EntityRuntime &EnsureRuntime(fr::Entity entity);

        /// Drop runtime when the entity is destroyed / animator removed.
        void ClearRuntime(fr::Entity entity);

      private:
        [[nodiscard]] bool resolveClipName(fr::Entity entity, std::string_view requested,
                                           std::string &outClipName) const;

        skr::Arc<fr::Registry> mRegistry;
        skr::Arc<AssetRegistry> mAssets;
        std::unordered_map<fr::Entity, EntityRuntime> mRuntimes;
    };

} // namespace FRIGGA_NAMESPACE
