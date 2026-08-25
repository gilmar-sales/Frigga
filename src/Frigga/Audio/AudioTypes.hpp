#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace FRIGGA_NAMESPACE
{

    struct AudioEventInstance
    {
        std::uint64_t id = 0;

        [[nodiscard]] bool IsValid() const
        {
            return id != 0;
        }
    };

    struct AudioBusHandle
    {
        std::uint64_t id = 0;

        [[nodiscard]] bool IsValid() const
        {
            return id != 0;
        }
    };

    struct AudioVcaHandle
    {
        std::uint64_t id = 0;

        [[nodiscard]] bool IsValid() const
        {
            return id != 0;
        }
    };

    struct AudioEventInfo
    {
        std::string path;
        float       minDurationSec = 0.0f;
        float       maxDurationSec = 0.0f;
        std::vector<std::string> parameters;
    };

    struct AudioBusInfo
    {
        std::string path;
        float       volume = 1.0f;
        bool        muted  = false;
    };

    struct AudioVcaInfo
    {
        std::string path;
        float       volume = 1.0f;
    };

    struct AudioClipTrim
    {
        float startSec = 0.0f;
        float endSec   = 0.0f;
    };

    struct WaveformData
    {
        std::vector<float> peaks;
        float              durationSec = 0.0f;
        int                sampleRate  = 0;
        int                channels    = 0;
    };

} // namespace FRIGGA_NAMESPACE
