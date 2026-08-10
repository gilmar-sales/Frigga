#include "ProjectSession.hpp"

#include "../Preferences/PreferencesStore.hpp"
#include "ProjectFile.hpp"
#include "ProjectScaffold.hpp"

#include <cstdlib>

namespace
{
    std::filesystem::path ExecutableDirectory()
    {
#if defined(__linux__)
        std::error_code ec;
        const auto self = std::filesystem::read_symlink("/proc/self/exe", ec);
        if(!ec)
        {
            return self.parent_path();
        }
#endif
        return std::filesystem::current_path();
    }

    bool LooksLikeFriggaRoot(const std::filesystem::path &path)
    {
        return std::filesystem::exists(path / "src/Frigga/Frigga.hpp") &&
               std::filesystem::exists(path / "CMakeLists.txt");
    }
} // namespace

ProjectSession::ProjectSession(skr::Arc<fg::Scene> scene,
                               skr::Arc<fg::GameplayPluginHost> pluginHost,
                               skr::Arc<fg::SceneSimulationState> simulation,
                               skr::Arc<EditorPreferences> preferences,
                               skr::Arc<skr::Logger<ProjectSession>> logger)
    : mScene(std::move(scene)), mPluginHost(std::move(pluginHost)),
      mSimulation(std::move(simulation)), mPreferences(std::move(preferences)),
      mLogger(std::move(logger))
{
}

std::filesystem::path ProjectSession::DiscoverFriggaBuild()
{
    const auto exeDir = ExecutableDirectory();
    if(std::filesystem::exists(exeDir / "libfrigga.a") ||
       std::filesystem::exists(exeDir / "libfriggad.a") ||
       std::filesystem::exists(exeDir / "Editor"))
    {
        return exeDir;
    }
    return std::filesystem::current_path();
}

std::filesystem::path ProjectSession::DiscoverFriggaRoot()
{
    const auto build = DiscoverFriggaBuild();
    const std::filesystem::path candidates[] = {
        build.parent_path(),
        build / "..",
        std::filesystem::current_path(),
        std::filesystem::current_path().parent_path(),
    };
    for(const auto &candidate : candidates)
    {
        std::error_code ec;
        const auto canonical = std::filesystem::weakly_canonical(candidate, ec);
        if(!ec && LooksLikeFriggaRoot(canonical))
        {
            return canonical;
        }
    }
    return build.parent_path();
}

bool ProjectSession::CreateProject(const std::filesystem::path &parentDir, std::string name,
                                   fg::SceneTemplate sceneTemplate)
{
    mLastError.clear();
    if(name.empty())
    {
        mLastError = "Enter a project name";
        return false;
    }

    ProjectDescriptor desc;
    desc.name           = std::move(name);
    desc.sceneTemplate  = sceneTemplate;
    desc.friggaRoot     = DiscoverFriggaRoot();
    desc.friggaBuild    = DiscoverFriggaBuild();
#ifdef _WIN32
    desc.pluginLibraryRelative = "build/" + desc.pluginTarget + ".dll";
#elif defined(__APPLE__)
    desc.pluginLibraryRelative = "build/lib" + desc.pluginTarget + ".dylib";
#else
    desc.pluginLibraryRelative = "build/lib" + desc.pluginTarget + ".so";
#endif

    const auto result = ProjectScaffold::Create(parentDir, desc, *mScene);
    if(!result.ok)
    {
        mLastError = result.error;
        mLogger->LogError("Scaffold failed: {}", mLastError);
        return false;
    }

    auto loaded = ProjectFile::Load(result.projectFile);
    if(!loaded)
    {
        mLastError = "Failed to re-load frigga.project after scaffold";
        return false;
    }

    return enterEditor(result.projectFile, std::move(*loaded));
}

bool ProjectSession::OpenProject(const std::filesystem::path &projectFile)
{
    mLastError.clear();
    auto loaded = ProjectFile::Load(projectFile);
    if(!loaded)
    {
        mLastError = "Invalid or missing frigga.project";
        return false;
    }

    const auto root      = projectFile.parent_path();
    const auto scenePath = root / loaded->sceneRelativePath;
    if(!mScene->LoadScene(scenePath))
    {
        mLastError = "Failed to load scene: " + scenePath.string();
        mLogger->LogError("{}", mLastError);
        return false;
    }

    return enterEditor(projectFile, std::move(*loaded));
}

void ProjectSession::CloseToHome()
{
    if(mSimulation->IsPlaying())
    {
        mSimulation->Stop();
    }
    UnloadPlugin();
    mProjectFile.reset();
    mDescriptor = {};
    mMode       = EditorSessionMode::Home;
    mStatusMessage.clear();
    mScene->NewScene();
}

bool ProjectSession::BuildPlugin()
{
    mLastError.clear();
    if(!mProjectFile)
    {
        mLastError = "No project open";
        return false;
    }

    const auto root     = mProjectFile->parent_path();
    const auto buildDir = root / "build-plugin";
    const auto cmakeCmd =
        "cmake -S \"" + root.string() + "\" -B \"" + buildDir.string() +
        "\" -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build \"" + buildDir.string() + "\"";

    mStatusMessage = "Building gameplay plugin…";
    mLogger->LogInformation("Building plugin: {}", cmakeCmd);
    const int code = std::system(cmakeCmd.c_str());
    if(code != 0)
    {
        mLastError     = "Plugin build failed (exit " + std::to_string(code) + ")";
        mStatusMessage = mLastError;
        mLogger->LogError("{}", mLastError);
        return false;
    }

    mStatusMessage = "Plugin build succeeded";
    return ReloadPlugin();
}

bool ProjectSession::ReloadPlugin()
{
    mLastError.clear();
    if(!mProjectFile)
    {
        mLastError = "No project open";
        return false;
    }

    const auto lib = pluginLibraryAbsolute();
    if(!mPluginHost->Load(lib))
    {
        mLastError     = mPluginHost->GetLastError();
        mStatusMessage = mLastError;
        return false;
    }

    mStatusMessage = "Loaded plugin " + lib.filename().string();
    return true;
}

void ProjectSession::UnloadPlugin()
{
    mPluginHost->Unload();
}

bool ProjectSession::enterEditor(const std::filesystem::path &projectFile, ProjectDescriptor desc)
{
    mProjectFile = projectFile;
    mDescriptor  = std::move(desc);
    mMode        = EditorSessionMode::Editor;
    touchRecent();

    // Best-effort plugin load (may be missing until first build).
    const auto lib = pluginLibraryAbsolute();
    if(std::filesystem::exists(lib))
    {
        if(mPluginHost->Load(lib))
        {
            mStatusMessage = "Opened " + mDescriptor.name;
        }
        else
        {
            mStatusMessage = "Opened " + mDescriptor.name + " (plugin not loaded)";
        }
    }
    else
    {
        mStatusMessage = "Opened " + mDescriptor.name + " — build the gameplay plugin to load code";
    }

    mLogger->LogInformation("Opened project {}", projectFile.string());
    return true;
}

void ProjectSession::touchRecent()
{
    if(!mProjectFile)
    {
        return;
    }
    PreferencesStore::TouchRecentProject(*mPreferences, *mProjectFile, mDescriptor.name);
    PreferencesStore::Save(*mPreferences);
}

std::filesystem::path ProjectSession::pluginLibraryAbsolute() const
{
    if(!mProjectFile)
    {
        return {};
    }
    return mProjectFile->parent_path() / mDescriptor.pluginLibraryRelative;
}
