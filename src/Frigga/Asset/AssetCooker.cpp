#include <Frigga/Asset/AssetCooker.hpp>

#include <algorithm>
#include <string>
#include <unordered_set>

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        bool IsUpToDate(const std::filesystem::path &source, const std::filesystem::path &target)
        {
            std::error_code ec;
            const auto sourceSize = std::filesystem::file_size(source, ec);
            if(ec)
            {
                return false;
            }
            const auto targetSize = std::filesystem::file_size(target, ec);
            if(ec || targetSize != sourceSize)
            {
                return false;
            }
            const auto sourceTime = std::filesystem::last_write_time(source, ec);
            if(ec)
            {
                return false;
            }
            const auto targetTime = std::filesystem::last_write_time(target, ec);
            return !ec && targetTime == sourceTime;
        }
    } // namespace

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

        // Incremental cook: keep the destination tree and skip files whose
        // size and timestamp already match the source.
        std::error_code ec;
        std::filesystem::create_directories(destination, ec);
        if(ec)
        {
            result.error = "Unable to create cooked resources: " + ec.message();
            return result;
        }

        std::vector<std::filesystem::path> files;
        for(const auto &entry: std::filesystem::recursive_directory_iterator(resourcesRoot, ec))
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
        std::unordered_set<std::string> createdDirs;
        for(const auto &source: files)
        {
            const auto relative = std::filesystem::relative(source, resourcesRoot, ec);
            if(ec)
            {
                result.error = "Unable to calculate cooked asset path: " + ec.message();
                return result;
            }
            const auto target = destination / relative;
            if(IsUpToDate(source, target))
            {
                continue;
            }
            const auto parent           = target.parent_path();
            const std::string parentKey = parent.generic_string();
            if(!createdDirs.contains(parentKey))
            {
                std::filesystem::create_directories(parent, ec);
                if(ec)
                {
                    result.error =
                        "Unable to cook asset '" + relative.generic_string() + "': " + ec.message();
                    return result;
                }
                createdDirs.insert(parentKey);
            }
            if(!std::filesystem::copy_file(source, target,
                                           std::filesystem::copy_options::overwrite_existing, ec) ||
               ec)
            {
                result.error = "Unable to cook asset '" + relative.generic_string() +
                               "': " + (ec ? ec.message() : "copy failed");
                return result;
            }
            // Preserve the source timestamp so the next cook can skip this file.
            const auto sourceTime = std::filesystem::last_write_time(source, ec);
            if(!ec)
            {
                std::filesystem::last_write_time(target, sourceTime, ec);
                ec.clear();
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
