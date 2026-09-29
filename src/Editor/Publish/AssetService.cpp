#include "AssetService.hpp"

#include <Frigga/Asset/AssetCooker.hpp>
#include <Frigga/Asset/AssetManifest.hpp>

namespace publish
{
    AssetValidation DefaultAssetService::ValidateManifest(
        const std::filesystem::path &resourcesRoot)
    {
        fg::AssetManifest manifest;
        std::string manifestError;
        if(!manifest.Load(resourcesRoot, &manifestError))
        {
            return {.ok = false, .error = std::move(manifestError)};
        }
        const auto validation = manifest.Validate(resourcesRoot);
        if(!validation.missing.empty())
        {
            return {.ok = false,
                    .error = "Asset manifest references missing files, e.g. '" +
                             validation.missing.front() + "'"};
        }
        return {.ok = true};
    }

    AssetCookOutcome DefaultAssetService::Cook(const std::filesystem::path &projectResources,
                                              const std::filesystem::path &destination)
    {
        const auto cooked = fg::AssetCooker::Cook(projectResources, destination);
        if(!cooked.ok)
        {
            return {.ok = false, .error = cooked.error};
        }
        return {.ok = true, .cookedCount = cooked.copied.size()};
    }
} // namespace publish
