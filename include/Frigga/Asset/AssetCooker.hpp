#pragma once

#include "Frigga/Asset/AssetManifest.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace FRIGGA_NAMESPACE
{
    struct AssetCookResult
    {
        bool ok = false;
        std::vector<std::string> copied;
        AssetManifest::ValidationResult validation;
        std::string error;
    };

    class AssetCooker
    {
      public:
        /// Validate the project manifest and copy its Resources tree to a
        /// deterministic cooked directory. Existing files are replaced.
        [[nodiscard]] static AssetCookResult Cook(
            const std::filesystem::path &resourcesRoot,
            const std::filesystem::path &destination);
    };
} // namespace FRIGGA_NAMESPACE
