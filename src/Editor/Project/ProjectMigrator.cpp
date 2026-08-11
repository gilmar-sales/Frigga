#include "ProjectMigrator.hpp"

#include "ProjectFile.hpp"
#include "ProjectScaffold.hpp"

#include <sstream>

namespace
{
    bool ApplyManagedLayout(const std::filesystem::path &projectRoot, ProjectDescriptor &desc,
                            std::string &error)
    {
        const auto written = ProjectScaffold::WriteManagedFiles(projectRoot, desc);
        if(!written.ok)
        {
            error = written.error;
            return false;
        }

        if(!ProjectScaffold::WriteExampleUserComponents(projectRoot, error))
        {
            return false;
        }

        if(!ProjectScaffold::MaybeRewriteManagedPluginEntry(projectRoot, error))
        {
            return false;
        }

        if(!ProjectScaffold::MaybeRewriteManagedGameplaySystem(projectRoot, error))
        {
            return false;
        }

        desc.formatVersion = ProjectDescriptor::CurrentFormatVersion;
        return true;
    }
} // namespace

ProjectMigrationResult ProjectMigrator::Migrate(const std::filesystem::path &projectFile,
                                                ProjectDescriptor &desc,
                                                const std::filesystem::path &friggaRoot,
                                                const std::filesystem::path &friggaBuild,
                                                bool force)
{
    ProjectMigrationResult result;
    result.fromVersion = desc.formatVersion > 0 ? desc.formatVersion
                                                : ProjectDescriptor::LegacyFormatVersion;
    result.toVersion   = ProjectDescriptor::CurrentFormatVersion;

    if(result.fromVersion > ProjectDescriptor::CurrentFormatVersion)
    {
        result.error =
            "Project format version " + std::to_string(result.fromVersion) +
            " is newer than this Editor supports (" +
            std::to_string(ProjectDescriptor::CurrentFormatVersion) + ")";
        return result;
    }

    if(!force && result.fromVersion >= ProjectDescriptor::CurrentFormatVersion)
    {
        result.ok      = true;
        result.message = "Project is already at format v" +
                         std::to_string(ProjectDescriptor::CurrentFormatVersion);
        return result;
    }

    if(!friggaRoot.empty())
    {
        desc.friggaRoot = friggaRoot;
    }
    if(!friggaBuild.empty())
    {
        desc.friggaBuild = friggaBuild;
    }
    if(desc.friggaRoot.empty() || desc.friggaBuild.empty())
    {
        result.error = "Engine paths missing; cannot migrate project scaffold";
        return result;
    }

    // v1 → v2: C++26 CMake + plugin header / README
    // v2 → v3: user-component helpers, Health example, managed plugin registration
    // v3 → v4: Health : fr::Component + FriRegister(registry, catalog) + Freyr Mutatons
    // v4 → v5: GameplaySystem : fr::System + late host DI registration
    // v5 → v6: register GameplaySystem on Simulation pipeline (Play-only)
    std::string stepError;
    if(!ApplyManagedLayout(projectFile.parent_path(), desc, stepError))
    {
        result.error = stepError;
        return result;
    }

    if(!ProjectFile::Save(projectFile, desc))
    {
        result.error = "Failed to write updated frigga.project";
        return result;
    }

    result.ok       = true;
    result.migrated = true;
    std::ostringstream msg;
    if(force && result.fromVersion >= ProjectDescriptor::CurrentFormatVersion)
    {
        msg << "Re-applied managed project files (format v" << result.toVersion << ")";
    }
    else
    {
        msg << "Migrated project format v" << result.fromVersion << " → v" << result.toVersion
            << " (Freyr gameplay systems via host DI, CMake C++26, plugin header)";
    }
    msg << ". Rebuild the gameplay plugin.";
    result.message = msg.str();
    return result;
}
