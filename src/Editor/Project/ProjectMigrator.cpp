#include "ProjectMigrator.hpp"

#include "ProjectEnginePaths.hpp"
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

        if(!ProjectScaffold::EnsureDefaultInputJson(projectRoot, error))
        {
            return false;
        }

        if(!ProjectScaffold::MaybeRewriteManagedModuleEntry(projectRoot, error))
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

    if(!force && result.fromVersion == ProjectDescriptor::CurrentFormatVersion)
    {
        result.ok      = true;
        result.message = "Project is already at format v" +
                         std::to_string(ProjectDescriptor::CurrentFormatVersion);
        return result;
    }

    if(!friggaRoot.empty())
    {
        desc.friggaRoot = friggaRoot;
        if(LooksLikeFriggaSdk(friggaRoot) || desc.friggaSdk.empty())
        {
            desc.friggaSdk = friggaRoot;
        }
    }
    if(!friggaBuild.empty())
    {
        desc.friggaBuild = friggaBuild;
        if(LooksLikeFriggaSdk(friggaBuild))
        {
            desc.friggaSdk = friggaBuild;
        }
    }
    FillMissingEnginePaths(desc);
    if(desc.friggaRoot.empty() || desc.friggaBuild.empty())
    {
        result.error = "Engine paths missing; cannot migrate project scaffold";
        return result;
    }

    // v1: modules/ (lowercase) with gameplay + extras; src/components + src/systems.
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
        msg << "Applied project format v" << result.toVersion
            << " (modules/ layout, FRI_MODULE)";
    }
    msg << ". Rebuild the gameplay module.";
    result.message = msg.str();
    return result;
}
