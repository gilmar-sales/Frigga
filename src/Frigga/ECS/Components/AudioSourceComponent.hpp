#pragma once

#include "Frigga/Audio/AudioTypes.hpp"
#include "Frigga/Audio/IAudioEngine.hpp"

#include <Freyr/Freyr.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>

namespace FRIGGA_NAMESPACE
{

    enum class AudioPlaybackState : std::uint8_t
    {
        Stopped = 0,
        Playing,
        Paused,
    };

    /// Authored audio emitter. Playback intents (`desired`, `oneShot`) and `instance`
    /// are runtime-only — AudioSystem owns engine sync; AudioController mutates intents.
    struct AudioSourceComponent: fr::Component
    {
        std::string eventPath;
        float       volume      = 1.0f;
        float       pitch       = 1.0f;
        bool        playOnAwake = false;
        bool        loop        = false;
        bool        is3D        = false;
        float       minDistance = 1.0f;
        float       maxDistance = 50.0f;

        /// Desired playback — written by AudioController / playOnAwake; applied by AudioSystem.
        AudioPlaybackState desired = AudioPlaybackState::Stopped;
        /// When true, System stops after the clip finishes (non-looping).
        bool oneShot = false;
        /// Cleared on Stop session; set when playOnAwake has been honored for this Play.
        bool awakeApplied = false;
        /// True after AudioSystem successfully started the engine instance this cycle.
        bool engineStarted = false;

        /// Runtime engine handle — only AudioSystem creates/releases.
        AudioEventInstance instance {};

        /// Ephemeral parameters applied when an instance exists (not serialized).
        std::unordered_map<std::string, float> parameters;
    };

    /// First active listener with a Transform wins; else AudioSystem falls back to main camera.
    struct AudioListenerComponent: fr::Component
    {
        bool active = true;
    };

} // namespace FRIGGA_NAMESPACE
