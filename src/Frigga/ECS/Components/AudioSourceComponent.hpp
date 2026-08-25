#pragma once

#include "Frigga/Audio/AudioTypes.hpp"
#include "Frigga/Audio/IAudioEngine.hpp"

#include <Freyr/Freyr.hpp>

#include <string>
#include <string_view>
#include <unordered_map>

namespace FRIGGA_NAMESPACE
{

    struct AudioSourceComponent: fr::Component
    {
        std::string eventPath;
        float       volume       = 1.0f;
        float       pitch        = 1.0f;
        bool        playOnAwake  = false;
        bool        loop         = false;
        bool        is3D         = true;
        float       minDistance  = 1.0f;
        float       maxDistance  = 50.0f;

        /// Runtime playback state — not serialized.
        AudioEventInstance instance {};
        bool               started     = false;
        bool               userPlaying = false;
    };

    struct AudioListenerComponent: fr::Component
    {
        bool active = true;
    };

} // namespace FRIGGA_NAMESPACE
