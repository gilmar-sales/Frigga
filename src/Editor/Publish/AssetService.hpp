#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

namespace publish
{
    struct AssetValidation
    {
        bool ok = false;
        std::string error;
    };

    struct AssetCookOutcome
    {
        bool ok = false;
        std::string error;
        std::size_t cookedCount = 0;
    };

    /// Injectable facade over the static fg::AssetManifest / fg::AssetCooker
    /// helpers so the publish pipeline can be composed through Skirnir DI
    /// and replaced by fakes in tests.
    class IAssetService
    {
      public:
        virtual ~IAssetService() = default;

        virtual AssetValidation ValidateManifest(
            const std::filesystem::path &resourcesRoot) = 0;
        virtual AssetCookOutcome Cook(const std::filesystem::path &projectResources,
                                      const std::filesystem::path &destination) = 0;
    };

    class DefaultAssetService final : public IAssetService
    {
      public:
        AssetValidation ValidateManifest(
            const std::filesystem::path &resourcesRoot) override;
        AssetCookOutcome Cook(const std::filesystem::path &projectResources,
                              const std::filesystem::path &destination) override;
    };
} // namespace publish
