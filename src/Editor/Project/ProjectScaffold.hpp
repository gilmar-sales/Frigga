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
    static constexpr std::string_view ManagedModuleMarker = "// FRIGGA_MANAGED_MODULE_ENTRY";
    static constexpr std::string_view ManagedGameplaySystemMarker =
        "// FRIGGA_MANAGED_GAMEPLAY_SYSTEM";
    static constexpr std::string_view ManagedModuleSubdirsBegin =
        "# FRIGGA_MANAGED_MODULE_SUBDIRS_BEGIN";
    static constexpr std::string_view ManagedModuleSubdirsEnd = "# FRIGGA_MANAGED_MODULE_SUBDIRS_END";

    /// Creates project directory under parentDir/name and writes all scaffold files.
    /// Uses @p scene to seed and serialize the template scene JSON.
    static ProjectScaffoldResult Create(const std::filesystem::path &parentDir,
                                        const ProjectDescriptor &desc,
                                        fg::Scene &scene);

    /// Rewrites Editor-managed files (CMakeLists, frigga_module.h, README,
    /// frigga_user_components.hpp, local CMakeUserPresets) without touching custom
    /// gameplay sources under modules/gameplay/src/.
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

    /// Creates `{project}/Resources/{Models,Textures,Prefabs,Fonts}` from the engine
    /// ProjectTemplate (plus optional engine default textures/font). Never overwrites
    /// files already in the project.
    static bool EnsureProjectResources(const std::filesystem::path &projectRoot,
                                       std::string &error,
                                       const std::filesystem::path &friggaRoot = {});

    /// Rewrites GameplayModule.cpp only if missing or still marked managed.
    static bool MaybeRewriteManagedModuleEntry(const std::filesystem::path &projectRoot,
                                               std::string &error);

    /// Rewrites GameplaySystem.* only if missing or still marked managed.
    static bool MaybeRewriteManagedGameplaySystem(const std::filesystem::path &projectRoot,
                                                  std::string &error);

    /// Scaffolds modules/<id>/ (CMake + FRI_MODULE) and registers it on @p desc.
    static bool CreateExtraModule(const std::filesystem::path &projectRoot, ProjectDescriptor &desc,
                                  std::string name, std::string &error);

    /// Copies a module tree into modules/<id> and registers it on @p desc.
    static bool InstallModule(const std::filesystem::path &projectRoot, ProjectDescriptor &desc,
                              const std::filesystem::path &sourceRoot, std::string &error);

    /// Rewrites the managed add_subdirectory block in the root CMakeLists.
    static bool SyncManagedModuleSubdirs(const std::filesystem::path &projectRoot,
                                         const ProjectDescriptor &desc, std::string &error);
};
