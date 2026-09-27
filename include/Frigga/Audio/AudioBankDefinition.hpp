#pragma once

#include <string>
#include <vector>

namespace FRIGGA_NAMESPACE
{

    /// One authored event inside an `.audiobank.json` file.
    /// Mirrors the schema consumed by `MiniaudioEngine::LoadBank`.
    struct AudioBankEventDef
    {
        std::string path; ///< e.g. "event:/SFX/Explosion"
        std::string clip; ///< relative to Resources/, e.g. "Audio/Clips/explosion.wav"
        float       volume = 1.0f;
        float       pitch  = 1.0f;
        bool        loop   = false;
        std::string bus    = "bus:/SFX";
    };

    /// Authored contents of one audio bank file.
    struct AudioBankDefinition
    {
        std::vector<AudioBankEventDef> events;
    };

} // namespace FRIGGA_NAMESPACE
