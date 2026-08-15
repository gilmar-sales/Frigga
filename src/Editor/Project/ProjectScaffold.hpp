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
    static constexpr std::string_view ManagedPluginSubdirsBegin =
        "# FRIGGA_MANAGED_PLUGIN_SUBDIRS_BEGIN";
    static constexpr std::string_view ManagedPluginSubdirsEnd = "# FRIGGA_MANAGED_PLUGIN_SUBDIRS_END";

    /// Creates project directory under parentDir/name and writes all scaffold files.
    /// Uses @p scene to seed and serialize the template scene JSON.
    static ProjectScaffoldResult Create(const std::filesystem::path &parentDir,
                                        const ProjectDescriptor &desc,
                                        fg::Scene &scene);

    /// Rewrites Editor-managed files (CMakeLists, frigga_plugin.h, README,
    /// frigga_user_components.hpp, local CMakeUserPresets) without touching custom
    /// gameplay sources under plugins/gameplay/src/.
    static ProjectManagedWriteResult WriteManagedFiles(const std::filesystem::path &projectRoot,
                                                       const ProjectDescriptor &desc);

    /// Local, gitignored preset with this machine's FRIGGA_SDK / ROOT / BUILD.
    static bool WriteCMakeUserPresets(const std::filesystem::path &projectRoot,
                                      const ProjectDescriptor &desc);

    /// Writes example Health component when missing (migration / create).
    static bool WriteExampleUserComponents(const std::filesystem::path &projectRoot,
                                           std::string &error);

    /// Writes default input.json when missing (never overwrites).
    static bool EnsureDefaultInputJson(const std::filesystem::path &projectRoot,
                                       std::string &error);

    /// Rewrites GameplayPlugin.cpp only if missing or still marked managed.
    static bool MaybeRewriteManagedPluginEntry(const std::filesystem::path &projectRoot,
                                               std::string &error);

    /// Rewrites GameplaySystem.* only if missing or still marked managed.
    static bool MaybeRewriteManagedGameplaySystem(const std::filesystem::path &projectRoot,
                                                  std::string &error);

    /// Scaffolds plugins/<id>/ (CMake + FRI_PLUGIN_MODULE) and registers it on @p desc.
    static bool CreateExtraPlugin(const std::filesystem::path &projectRoot, ProjectDescriptor &desc,
                                  std::string name, std::string &error);

    /// Copies a plugin tree into plugins/<id> and registers it on @p desc.
    static bool InstallPlugin(const std::filesystem::path &projectRoot, ProjectDescriptor &desc,
                              const std::filesystem::path &sourceRoot, std::string &error);

    /// Rewrites the managed add_subdirectory block in the root CMakeLists.
    static bool SyncManagedPluginSubdirs(const std::filesystem::path &projectRoot,
                                         const ProjectDescriptor &desc, std::string &error);
};
