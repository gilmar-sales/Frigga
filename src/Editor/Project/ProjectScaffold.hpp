#pragma once

#include "ProjectDescriptor.hpp"

#include <Frigga/Scene/Scene.hpp>

#include <filesystem>
#include <string>
#include <string_view>

struct ProjectScaffoldResult
{
    bool ok = false;
    std::filesystem::path projectFile;
    std::string error;
};

struct ProjectManagedWriteResult
{
    bool ok = false;
    std::string error;
};

class ProjectScaffold
{
  public:
    static constexpr std::string_view ManagedPluginMarker = "// FRIGGA_MANAGED_PLUGIN_ENTRY";
    static constexpr std::string_view ManagedGameplaySystemMarker =
        "// FRIGGA_MANAGED_GAMEPLAY_SYSTEM";

    /// Creates project directory under parentDir/name and writes all scaffold files.
    /// Uses @p scene to seed and serialize the template scene JSON.
    static ProjectScaffoldResult Create(const std::filesystem::path &parentDir,
                                        const ProjectDescriptor &desc,
                                        fg::Scene &scene);

    /// Rewrites Editor-managed files (CMakeLists, frigga_plugin.h, README,
    /// frigga_user_components.hpp) without touching custom gameplay sources under src/.
    static ProjectManagedWriteResult WriteManagedFiles(const std::filesystem::path &projectRoot,
                                                       const ProjectDescriptor &desc);

    /// Writes example Health component when missing (migration / create).
    static bool WriteExampleUserComponents(const std::filesystem::path &projectRoot,
                                           std::string &error);

    /// Rewrites GameplayPlugin.cpp only if missing or still marked managed.
    static bool MaybeRewriteManagedPluginEntry(const std::filesystem::path &projectRoot,
                                               std::string &error);

    /// Rewrites GameplaySystem.* only if missing or still marked managed.
    static bool MaybeRewriteManagedGameplaySystem(const std::filesystem::path &projectRoot,
                                                  std::string &error);
};
