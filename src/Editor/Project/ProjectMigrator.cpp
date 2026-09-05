#include "ProjectMigrator.hpp"

#include "ProjectEnginePaths.hpp"
#include "ProjectFile.hpp"
#include "ProjectScaffold.hpp"

#include <algorithm>
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

        if(!ProjectScaffold::EnsureDefaultInputJson(projectRoot, error))
        {
            return false;
        }

        const auto hasGameplay = std::ranges::any_of(
            desc.modules, [](const ProjectModuleEntry &entry) { return entry.IsGameplay(); });
        if(hasGameplay &&
           (!ProjectScaffold::WriteExampleUserComponents(projectRoot, error) ||
            !ProjectScaffold::MaybeRewriteManagedModuleEntry(projectRoot, error) ||
            !ProjectScaffold::MaybeRewriteManagedGameplaySystem(projectRoot, error)))
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

    const auto projectRoot = projectFile.parent_path();
    const auto legacyModules = projectRoot / "modules";
    const auto modules        = projectRoot / ProjectDescriptor::ModulesDirName;
    const bool needsModuleRename =
        std::filesystem::exists(legacyModules) && !std::filesystem::exists(modules);
    const auto legacyScenes = projectRoot / "scenes";
    const auto nestedScenes = projectRoot / "Resources" / "Scenes";
    const auto scenes       = projectRoot / "Scenes";
    const bool needsSceneRename =
        (std::filesystem::exists(legacyScenes) || std::filesystem::exists(nestedScenes)) &&
        !std::filesystem::exists(scenes);
    if(!force && result.fromVersion == ProjectDescriptor::CurrentFormatVersion &&
       !needsModuleRename && !needsSceneRename)
    {
        result.ok      = true;
        result.message = "Project is already at format v" +
                         std::to_string(ProjectDescriptor::CurrentFormatVersion);
        return result;
    }

    if(std::filesystem::exists(legacyModules) && std::filesystem::exists(modules))
    {
        result.error = "Both modules/ and Modules/ exist; merge them before opening the project";
        return result;
    }
    if(needsModuleRename)
    {
        std::error_code ec;
        std::filesystem::rename(legacyModules, modules, ec);
        if(ec)
        {
            result.error = "Failed to migrate modules/ to Modules/: " + ec.message();
            return result;
        }
    }
    if((std::filesystem::exists(legacyScenes) && std::filesystem::exists(nestedScenes)) ||
       ((std::filesystem::exists(legacyScenes) || std::filesystem::exists(nestedScenes)) &&
        std::filesystem::exists(scenes)))
    {
        result.error = "Multiple scene folders exist; merge them into Scenes/ before opening the project";
        return result;
    }
    if(needsSceneRename)
    {
        std::error_code ec;
        const auto source = std::filesystem::exists(legacyScenes) ? legacyScenes : nestedScenes;
        std::filesystem::create_directories(scenes.parent_path(), ec);
        if(!ec)
        {
            std::filesystem::rename(source, scenes, ec);
        }
        if(ec)
        {
            result.error = "Failed to migrate scenes to Scenes/: " + ec.message();
            return result;
        }
    }
    if(desc.sceneRelativePath == "scenes/main.json" ||
       desc.sceneRelativePath.starts_with("scenes/") ||
       desc.sceneRelativePath == "Resources/Scenes/main.json" ||
       desc.sceneRelativePath.starts_with("Resources/Scenes/"))
    {
        const auto prefix = desc.sceneRelativePath.starts_with("Resources/Scenes/")
                                ? std::string("Resources/Scenes/")
                                : std::string("scenes/");
        desc.sceneRelativePath = "Scenes/" + desc.sceneRelativePath.substr(prefix.size());
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

    if(result.fromVersion < ProjectDescriptor::CurrentFormatVersion && desc.modules.empty())
    {
        desc.EnsureGameplayModule();
    }

    // Legacy projects used modules/ (lowercase); current projects use Modules/.
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
            << " (Modules/ layout, FRI_MODULE)";
    }
    if(!desc.modules.empty())
    {
        msg << ". Rebuild the project modules.";
    }
    result.message = msg.str();
    return result;
}
