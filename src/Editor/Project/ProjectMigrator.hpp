#pragma once

#include "ProjectDescriptor.hpp"

#include <filesystem>
#include <string>

struct ProjectMigrationResult
{
    bool ok        = false;
    bool migrated  = false;
    int  fromVersion = 0;
    int  toVersion   = 0;
    std::string message;
    std::string error;
};

class ProjectMigrator
{
  public:
    /**
     * Upgrades a project on disk to ProjectDescriptor::CurrentFormatVersion when needed.
     * Refreshes engine paths, rewrites managed scaffold files (CMakeLists, plugin header, README),
     * and bumps frigga.project version. Never overwrites src/Gameplay*.
     *
     * @param force when true, rewrite managed files even if already at current version.
     */
    static ProjectMigrationResult Migrate(const std::filesystem::path &projectFile,
                                          ProjectDescriptor &desc,
                                          const std::filesystem::path &friggaRoot,
                                          const std::filesystem::path &friggaBuild,
                                          bool force = false);
};
