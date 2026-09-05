#include <Frigga/Asset/AssetCooker.hpp>

#include <algorithm>

namespace FRIGGA_NAMESPACE
{
    AssetCookResult AssetCooker::Cook(const std::filesystem::path &resourcesRoot,
                                      const std::filesystem::path &destination)
    {
        AssetCookResult result;
        AssetManifest manifest;
        if(!manifest.Load(resourcesRoot, &result.error))
        {
            return result;
        }
        result.validation = manifest.Validate(resourcesRoot);
        if(!result.validation.missing.empty())
        {
            result.error = "Cannot cook resources with missing manifest assets";
            return result;
        }

        std::error_code ec;
        std::filesystem::remove_all(destination, ec);
        if(ec)
        {
            result.error = "Unable to clear cooked resources: " + ec.message();
            return result;
        }
        std::filesystem::create_directories(destination, ec);
        if(ec)
        {
            result.error = "Unable to create cooked resources: " + ec.message();
            return result;
        }

        std::vector<std::filesystem::path> files;
        for(const auto &entry :
            std::filesystem::recursive_directory_iterator(resourcesRoot, ec))
        {
            if(ec)
            {
                result.error = "Unable to scan resources: " + ec.message();
                return result;
            }
            if(entry.is_regular_file(ec) && entry.path().filename() != AssetManifest::FileName)
            {
                files.push_back(entry.path());
            }
        }
        std::ranges::sort(files);
        for(const auto &source : files)
        {
            const auto relative = std::filesystem::relative(source, resourcesRoot, ec);
            if(ec)
            {
                result.error = "Unable to calculate cooked asset path: " + ec.message();
                return result;
            }
            const auto target = destination / relative;
            std::filesystem::create_directories(target.parent_path(), ec);
            if(ec || !std::filesystem::copy_file(
                          source, target, std::filesystem::copy_options::overwrite_existing, ec))
            {
                result.error = "Unable to cook asset '" + relative.generic_string() + "': " +
                               (ec ? ec.message() : "copy failed");
                return result;
            }
            result.copied.push_back(relative.generic_string());
        }

        if(!manifest.Save(destination, &result.error))
        {
            return result;
        }
        result.ok = true;
        return result;
    }
} // namespace FRIGGA_NAMESPACE
