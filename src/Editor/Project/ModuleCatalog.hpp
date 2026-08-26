#pragma once

#include "ProjectDescriptor.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct DiscoveredModule
{
    std::string id;
    std::string name;
    std::string target;
    std::string libraryRelative;
    std::filesystem::path root;
    bool bundled = false;
};

class ModuleCatalog
{
  public:
    static constexpr const char *ManifestFileName = "module.json";

    [[nodiscard]] static std::string SanitizeId(std::string_view raw);
    [[nodiscard]] static std::optional<DiscoveredModule> ReadManifest(
        const std::filesystem::path &moduleRoot);
    static bool WriteManifest(const std::filesystem::path &moduleRoot, const DiscoveredModule &module);

    [[nodiscard]] static std::vector<DiscoveredModule> ScanDirectory(
        const std::filesystem::path &dir, bool bundled = false);

    [[nodiscard]] static std::vector<std::filesystem::path> BundledModuleSearchDirs(
        const std::filesystem::path &friggaSdk, const std::filesystem::path &friggaRoot,
        const std::filesystem::path &executableDir);

    [[nodiscard]] static std::vector<DiscoveredModule> ScanBundled(
        const std::filesystem::path &friggaSdk, const std::filesystem::path &friggaRoot,
        const std::filesystem::path &executableDir);

    static bool CopyModuleTree(const std::filesystem::path &from, const std::filesystem::path &to,
                               std::string &error);
};
