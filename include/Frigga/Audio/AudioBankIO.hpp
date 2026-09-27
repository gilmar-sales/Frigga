#pragma once

#include "Frigga/Audio/AudioBankDefinition.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace FRIGGA_NAMESPACE
{

    /// Parse `.audiobank.json` @p json. Missing optional fields fall back to the
    /// `AudioBankEventDef` defaults. Returns false with `*error` set on malformed JSON.
    [[nodiscard]] bool ParseAudioBankDefinition(std::string_view json, AudioBankDefinition &out,
                                                std::string *error = nullptr);

    /// Serialize @p bank to canonical `.audiobank.json` text (round-trips through
    /// ParseAudioBankDefinition and is readable by MiniaudioEngine::LoadBank).
    [[nodiscard]] std::string SerializeAudioBankDefinition(const AudioBankDefinition &bank);

    [[nodiscard]] bool LoadAudioBankFile(const std::filesystem::path &absolutePath,
                                         AudioBankDefinition &out, std::string *error = nullptr);

    /// Creates parent directories as needed and overwrites @p absolutePath.
    [[nodiscard]] bool SaveAudioBankFile(const std::filesystem::path &absolutePath,
                                         const AudioBankDefinition &bank,
                                         std::string *error = nullptr);

} // namespace FRIGGA_NAMESPACE
