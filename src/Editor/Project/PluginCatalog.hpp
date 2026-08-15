#pragma once

#include "ProjectDescriptor.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct DiscoveredPlugin
{
    std::string id;
    std::string name;
    std::string target;
    std::string libraryRelative;
    std::filesystem::path root;
    bool bundled = false;
};

class PluginCatalog
{
  public:
    static constexpr const char *ManifestFileName = "plugin.json";

    [[nodiscard]] static std::string SanitizeId(std::string_view raw);
    [[nodiscard]] static std::optional<DiscoveredPlugin> ReadManifest(
        const std::filesystem::path &pluginRoot);
    static bool WriteManifest(const std::filesystem::path &pluginRoot, const DiscoveredPlugin &plugin);

    [[nodiscard]] static std::vector<DiscoveredPlugin> ScanDirectory(
        const std::filesystem::path &dir, bool bundled = false);

    [[nodiscard]] static std::vector<std::filesystem::path> BundledPluginSearchDirs(
        const std::filesystem::path &friggaSdk, const std::filesystem::path &friggaRoot,
        const std::filesystem::path &executableDir);

    [[nodiscard]] static std::vector<DiscoveredPlugin> ScanBundled(
        const std::filesystem::path &friggaSdk, const std::filesystem::path &friggaRoot,
        const std::filesystem::path &executableDir);

    static bool CopyPluginTree(const std::filesystem::path &from, const std::filesystem::path &to,
                               std::string &error);
};
