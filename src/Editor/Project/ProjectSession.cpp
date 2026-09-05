#include "ProjectSession.hpp"

#include "../EditorWindowLayout.hpp"
#include "../Preferences/PreferencesStore.hpp"
#include "ProjectFile.hpp"
#include "ProjectMigrator.hpp"
#include "ProjectScaffold.hpp"
#include "ModuleCatalog.hpp"
#include "ProjectEnginePaths.hpp"

#include <Frigga/Asset/AssetRegistry.hpp>
#include <Frigga/ECS/EcsLayout.hpp>
#include <Frigga/Input/InputMapIO.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <sstream>
#include <string_view>

#if defined(_WIN32)
#    include <stdio.h>
#    include <windows.h>
#elif defined(__APPLE__)
#    include <mach-o/dyld.h>
#    include <cstdint>
#else
#    include <spawn.h>
#    include <sys/wait.h>
#    include <unistd.h>
extern char **environ;
#endif

namespace
{
    std::filesystem::path ExecutableDirectory()
    {
        const auto path = ProjectSession::ExecutablePath();
        return path.empty() ? std::filesystem::current_path() : path.parent_path();
    }

    std::filesystem::path RuntimeExecutablePath()
    {
#ifdef _WIN32
        return ExecutableDirectory() / "Runtime.exe";
#else
        return ExecutableDirectory() / "Runtime";
#endif
    }

    std::string EscapeJson(std::string_view value)
    {
        std::string out;
        out.reserve(value.size() + 8);
        for(const char ch : value)
        {
            switch(ch)
            {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out.push_back(ch);
                break;
            }
        }
        return out;
    }

    std::string FormatUtcTimestamp()
    {
        using clock = std::chrono::system_clock;
        const auto now = clock::now();
        const auto tt  = clock::to_time_t(now);
        std::tm tm {};
#if defined(_WIN32)
        gmtime_s(&tm, &tt);
#else
        gmtime_r(&tt, &tm);
#endif
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02dZ", tm.tm_year + 1900,
                      tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        return buffer;
    }

    bool LooksLikeFriggaRoot(const std::filesystem::path &path)
    {
        return LooksLikeFriggaEngineRoot(path);
    }

    /// Parse Ninja-style "[12/74]" progress. Returns true when a fraction was found.
    bool TryParseNinjaProgress(std::string_view line, float &outProgress)
    {
        const auto open = line.find('[');
        if(open == std::string_view::npos)
        {
            return false;
        }
        const auto slash = line.find('/', open + 1);
        if(slash == std::string_view::npos)
        {
            return false;
        }
        const auto close = line.find(']', slash + 1);
        if(close == std::string_view::npos)
        {
            return false;
        }

        int current = 0;
        int total   = 0;
        try
        {
            current = std::stoi(std::string(line.substr(open + 1, slash - open - 1)));
            total   = std::stoi(std::string(line.substr(slash + 1, close - slash - 1)));
        }
        catch(...)
        {
            return false;
        }

        if(total <= 0)
        {
            return false;
        }
        outProgress = static_cast<float>(current) / static_cast<float>(total);
        return true;
    }

    int RunShellCapturing(const std::string &command,
                          const std::function<void(std::string_view)> &onLine)
    {
        const std::string wrapped = command + " 2>&1";
#ifdef _WIN32
        FILE *pipe = _popen(wrapped.c_str(), "r");
#else
        FILE *pipe = popen(wrapped.c_str(), "r");
#endif
        if(!pipe)
        {
            return -1;
        }

        std::array<char, 512> buffer {};
        while(fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
        {
            onLine(buffer.data());
        }

#ifdef _WIN32
        return _pclose(pipe);
#else
        const int status = pclose(pipe);
        if(status == -1)
        {
            return -1;
        }
        if(WIFEXITED(status))
        {
            return WEXITSTATUS(status);
        }
        return -1;
#endif
    }

    /// Prefer the same C++ compiler the Frigga Editor was built with.
    std::string ReadCMakeCacheValue(const std::filesystem::path &cacheFile, std::string_view key)
    {
        if(!std::filesystem::exists(cacheFile))
        {
            return {};
        }
        std::ifstream file(cacheFile);
        std::string   line;
        const std::string prefix = std::string(key) + ":";
        while(std::getline(file, line))
        {
            if(line.rfind(prefix, 0) != 0)
            {
                continue;
            }
            const auto eq = line.find('=');
            if(eq == std::string::npos)
            {
                continue;
            }
            return line.substr(eq + 1);
        }
        return {};
    }

    std::filesystem::path FirstExistingCMakeCache(
        std::initializer_list<std::filesystem::path> dirs)
    {
        for(const auto &dir : dirs)
        {
            if(dir.empty())
            {
                continue;
            }
            const auto cache = dir / "CMakeCache.txt";
            if(std::filesystem::exists(cache))
            {
                return cache;
            }
        }
        return {};
    }
} // namespace

ProjectSession::ProjectSession(skr::Arc<fg::Scene> scene,
                               skr::Arc<fg::GameplayModuleHost> moduleHost,
                               skr::Arc<fg::SceneSimulationState> simulation,
                               skr::Arc<fg::Input> input,
                               skr::Arc<fg::AssetRegistry> assets,
                               skr::Arc<fr::Registry> registry,
                               skr::Arc<EditorPreferences> preferences,
                               skr::Arc<fra::Window> window,
                               skr::Arc<skr::Logger<ProjectSession>> logger)
    : mScene(std::move(scene)), mModuleHost(std::move(moduleHost)),
      mSimulation(std::move(simulation)), mInput(std::move(input)), mAssets(std::move(assets)),
      mRegistry(std::move(registry)), mPreferences(std::move(preferences)),
      mWindow(std::move(window)), mLogger(std::move(logger))
{
}

ProjectSession::~ProjectSession()
{
    unbindProjectResources();
    clearEditorSessionMarker();
    joinBuildThread();
}

std::filesystem::path ProjectSession::ExecutablePath()
{
#if defined(_WIN32)
    char buffer[MAX_PATH];
    const DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if(len > 0 && len < MAX_PATH)
    {
        return std::filesystem::path(buffer);
    }
#elif defined(__linux__)
    std::error_code ec;
    const auto self = std::filesystem::read_symlink("/proc/self/exe", ec);
    if(!ec)
    {
        return self;
    }
#elif defined(__APPLE__)
    char buffer[4096];
    uint32_t size = sizeof(buffer);
    if(_NSGetExecutablePath(buffer, &size) == 0)
    {
        std::error_code ec;
        const auto canonical = std::filesystem::weakly_canonical(buffer, ec);
        if(!ec)
        {
            return canonical;
        }
        return std::filesystem::path(buffer);
    }
#endif
    return {};
}

void ProjectSession::joinBuildThread()
{
    if(mBuildThread.joinable())
    {
        mBuildThread.join();
    }
    mBuildRunning.store(false, std::memory_order_release);
}

std::string ProjectSession::GetStatusMessage() const
{
    std::lock_guard lock(mMutex);
    return mStatusMessage;
}

std::string ProjectSession::GetLastError() const
{
    std::lock_guard lock(mMutex);
    return mLastError;
}

ModuleBuildPhase ProjectSession::GetBuildPhase() const
{
    return mBuildPhase.load(std::memory_order_acquire);
}

float ProjectSession::GetBuildProgress() const
{
    if(mBuildProgressDeterminate.load(std::memory_order_acquire))
    {
        return std::clamp(mBuildProgress.load(std::memory_order_acquire), 0.0f, 1.0f);
    }

    // Smooth indeterminate pulse for the UI while configure / unknown build runs.
    using clock     = std::chrono::steady_clock;
    const auto ms   = std::chrono::duration_cast<std::chrono::milliseconds>(
                        clock::now().time_since_epoch())
                        .count();
    const float wave = 0.5f + 0.5f * std::sin(static_cast<float>(ms) * 0.004f);
    return wave * 0.85f;
}

bool ProjectSession::IsBuildProgressDeterminate() const
{
    return mBuildProgressDeterminate.load(std::memory_order_acquire);
}

std::string ProjectSession::GetBuildLogTail() const
{
    std::lock_guard lock(mMutex);
    return mBuildLogTail;
}

std::vector<EditorBackgroundTask> ProjectSession::GetBackgroundTasks() const
{
    const auto phase = GetBuildPhase();
    if(phase == ModuleBuildPhase::Idle)
    {
        return {};
    }

    EditorBackgroundTask task;
    const bool publishing = mPublishing.load(std::memory_order_acquire) ||
                            mLastOperationWasPublish.load(std::memory_order_acquire);
    const char *title       = publishing ? "Publish game" : "Build gameplay module";
    task.id                 = publishing ? "game-publish" : "gameplay-module-build";

    switch(phase)
    {
    case ModuleBuildPhase::Configuring:
        task.title  = title;
        task.detail = "Configuring (CMake)…";
        task.state   = EditorBackgroundTaskState::Running;
        break;
    case ModuleBuildPhase::Building:
        task.title  = title;
        task.detail = "Compiling…";
        task.state   = EditorBackgroundTaskState::Running;
        break;
    case ModuleBuildPhase::Reloading:
        task.title  = title;
        task.detail = "Reloading module…";
        task.state   = EditorBackgroundTaskState::Running;
        break;
    case ModuleBuildPhase::Succeeded:
        task.title  = title;
        task.detail = "Succeeded";
        task.state   = EditorBackgroundTaskState::Succeeded;
        break;
    case ModuleBuildPhase::Failed:
        task.title  = "Build gameplay module";
        task.detail = GetLastError().empty() ? "Failed" : GetLastError();
        task.state   = EditorBackgroundTaskState::Failed;
        break;
    default:
        return {};
    }

    task.progress    = GetBuildProgress();
    task.determinate = IsBuildProgressDeterminate() ||
                       phase == ModuleBuildPhase::Succeeded || phase == ModuleBuildPhase::Failed;
    if(phase == ModuleBuildPhase::Succeeded || phase == ModuleBuildPhase::Failed)
    {
        task.progress = 1.0f;
    }
    task.logTail = GetBuildLogTail();
    return {std::move(task)};
}

bool ProjectSession::HasRunningBackgroundTasks() const
{
    const auto phase = GetBuildPhase();
    return IsBuilding() || phase == ModuleBuildPhase::Reloading;
}

void ProjectSession::writeEditorSessionMarker()
{
    if(!mProjectFile)
    {
        return;
    }

    const auto projectRoot = mProjectFile->parent_path();
    const auto markerDir   = projectRoot / ".frigga";
    const auto markerPath  = markerDir / "editor-session.json";

    std::error_code ec;
    std::filesystem::create_directories(markerDir, ec);
    if(ec)
    {
        mLogger->LogWarning("Failed to create {}: {}", markerDir.string(), ec.message());
        return;
    }

    const auto editorPath = ExecutablePath();
    const auto soSearch   = (projectRoot / "build").lexically_normal();
    const auto moduleLib  = moduleLibraryAbsolute().lexically_normal();

#if defined(_WIN32)
    const auto pid = static_cast<unsigned long long>(GetCurrentProcessId());
#else
    const auto pid = static_cast<unsigned long long>(::getpid());
#endif

    std::ostringstream json;
    json << "{\n";
    json << "  \"pid\": " << pid << ",\n";
    json << "  \"editorPath\": \"" << EscapeJson(editorPath.generic_string()) << "\",\n";
    json << "  \"soSearchPath\": \"" << EscapeJson(soSearch.generic_string()) << "\",\n";
    json << "  \"moduleLibrary\": \"" << EscapeJson(moduleLib.generic_string()) << "\",\n";
    json << "  \"projectRoot\": \"" << EscapeJson(projectRoot.generic_string()) << "\",\n";
    json << "  \"updatedAt\": \"" << FormatUtcTimestamp() << "\"\n";
    json << "}\n";

    std::ofstream file(markerPath, std::ios::binary | std::ios::trunc);
    if(!file)
    {
        mLogger->LogWarning("Failed to write editor session marker {}", markerPath.string());
        return;
    }
    file << json.str();
}

void ProjectSession::clearEditorSessionMarker()
{
    if(!mProjectFile)
    {
        return;
    }

    const auto markerPath = mProjectFile->parent_path() / ".frigga" / "editor-session.json";
    std::error_code ec;
    std::filesystem::remove(markerPath, ec);
}

void ProjectSession::Poll()
{
    if(!mBuildFinished.exchange(false, std::memory_order_acq_rel))
    {
        return;
    }

    joinBuildThread();

    const int exitCode = mBuildExitCode.load(std::memory_order_acquire);
    const bool wasPublishing = mPublishing.exchange(false, std::memory_order_acq_rel);
    mLastOperationWasPublish.store(wasPublishing, std::memory_order_release);
    if(exitCode != 0)
    {
        mBuildPhase.store(ModuleBuildPhase::Failed, std::memory_order_release);
        std::lock_guard lock(mMutex);
        mLastError     = (wasPublishing ? "Game publication failed (exit "
                                        : "Module build failed (exit ") +
                       std::to_string(exitCode) + ")";
        mStatusMessage = mLastError;
        mLogger->LogError("{}", mLastError);
        return;
    }

    if(mReloadAfterBuild)
    {
        mBuildPhase.store(ModuleBuildPhase::Reloading, std::memory_order_release);
        mBuildProgress.store(0.95f, std::memory_order_release);
        mBuildProgressDeterminate.store(true, std::memory_order_release);

        if(ReloadModule())
        {
            mBuildPhase.store(ModuleBuildPhase::Succeeded, std::memory_order_release);
            mBuildProgress.store(1.0f, std::memory_order_release);
            if(mPendingStartupScene && !tryLoadPendingStartupScene())
            {
                mBuildPhase.store(ModuleBuildPhase::Failed, std::memory_order_release);
            }
            // ReloadModule already set mStatusMessage with the registered type list.
        }
        else
        {
            mBuildPhase.store(ModuleBuildPhase::Failed, std::memory_order_release);
        }
    }
    else
    {
        mBuildPhase.store(ModuleBuildPhase::Succeeded, std::memory_order_release);
        mBuildProgress.store(1.0f, std::memory_order_release);
        if(wasPublishing)
        {
            std::lock_guard lock(mMutex);
            mStatusMessage = "Game published to " + mPublishDestination.string();
        }
    }

    mReloadAfterBuild = false;
}

std::filesystem::path ProjectSession::DiscoverFriggaBuild()
{
    const auto exeDir = ExecutableDirectory();
    const auto sdk    = exeDir / "Sdk";
    if(LooksLikeFriggaSdk(sdk))
    {
        std::error_code ec;
        const auto canonical = std::filesystem::weakly_canonical(sdk, ec);
        return ec ? sdk : canonical;
    }
    if(std::filesystem::exists(exeDir / "libfrigga.a") ||
       std::filesystem::exists(exeDir / "libfriggad.a") ||
       std::filesystem::exists(exeDir / "Editor") ||
       std::filesystem::exists(exeDir / "Editor.exe"))
    {
        return exeDir;
    }
    return std::filesystem::current_path();
}

std::filesystem::path ProjectSession::DiscoverFriggaRoot()
{
    const auto exeDir = ExecutableDirectory();
    const auto sdk    = exeDir / "Sdk";
    if(LooksLikeFriggaSdk(sdk))
    {
        std::error_code ec;
        const auto canonical = std::filesystem::weakly_canonical(sdk, ec);
        return ec ? sdk : canonical;
    }

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

std::filesystem::path ProjectSession::DiscoverFriggaSdk()
{
    const auto exeDir = ExecutableDirectory();
    const auto sdk    = exeDir / "Sdk";
    if(LooksLikeFriggaSdk(sdk))
    {
        std::error_code ec;
        const auto canonical = std::filesystem::weakly_canonical(sdk, ec);
        return ec ? sdk : canonical;
    }
    return DiscoverFriggaRoot();
}

bool ProjectSession::CreateProject(const std::filesystem::path &parentDir, std::string name,
                                   fg::SceneTemplate sceneTemplate)
{
    {
        std::lock_guard lock(mMutex);
        mLastError.clear();
    }
    if(name.empty())
    {
        std::lock_guard lock(mMutex);
        mLastError = "Enter a project name";
        return false;
    }

    ProjectDescriptor desc;
    desc.name          = std::move(name);
    desc.sceneTemplate = sceneTemplate;
    desc.friggaRoot    = DiscoverFriggaRoot();
    desc.friggaBuild   = DiscoverFriggaBuild();
    desc.friggaSdk     = DiscoverFriggaSdk();
#ifdef _WIN32
    desc.moduleLibraryRelative = "build/" + desc.moduleTarget + ".dll";
#elif defined(__APPLE__)
    desc.moduleLibraryRelative = "build/lib" + desc.moduleTarget + ".dylib";
#else
    desc.moduleLibraryRelative = "build/lib" + desc.moduleTarget + ".so";
#endif
    desc.EnsureGameplayModule();

    const auto result = ProjectScaffold::Create(parentDir, desc, *mScene);
    if(!result.ok)
    {
        std::lock_guard lock(mMutex);
        mLastError = result.error;
        mLogger->LogError("Scaffold failed: {}", mLastError);
        return false;
    }

    auto loaded = ProjectFile::Load(result.projectFile);
    if(!loaded)
    {
        std::lock_guard lock(mMutex);
        mLastError = "Failed to re-load frigga.project after scaffold";
        return false;
    }

    mDescriptor = *loaded;
    bindProjectResources(result.projectFile.parent_path());
    if(!enterEditor(result.projectFile, std::move(*loaded)))
    {
        return false;
    }
    if(anyEnabledModuleMissing())
    {
        BuildModule();
    }
    return true;
}

bool ProjectSession::OpenProject(const std::filesystem::path &projectFile)
{
    {
        std::lock_guard lock(mMutex);
        mLastError.clear();
    }
    auto loaded = ProjectFile::Load(projectFile);
    if(!loaded)
    {
        std::lock_guard lock(mMutex);
        mLastError = "Invalid or missing frigga.project";
        return false;
    }

    if(!migrateProjectFile(projectFile, *loaded, false))
    {
        return false;
    }

    // Register gameplay types before scene deserialize so userComponents apply immediately.
    mProjectFile = projectFile;
    mDescriptor  = *loaded;
    mDescriptor.EnsureGameplayModule();

    const auto root      = projectFile.parent_path();
    const auto scenePath = root / loaded->sceneRelativePath;
    bindProjectResources(root);

    if(anyEnabledModuleMissing())
    {
        mPendingStartupScene = scenePath;
        mScene->NewScene();
        if(!enterEditor(projectFile, std::move(*loaded), /*loadModule=*/false))
        {
            mPendingStartupScene.reset();
            mModuleHost->Unload();
            unbindProjectResources();
            mProjectFile.reset();
            mDescriptor = {};
            return false;
        }
        BuildModule();
        {
            std::lock_guard lock(mMutex);
            mStatusMessage = "Building modules before loading scene…";
        }
        return true;
    }

    if(!loadEnabledModules())
    {
        mLogger->LogWarning("Modules missing or failed to load before scene: {}",
                            mModuleHost->GetLastError());
    }

    if(!mScene->LoadScene(scenePath))
    {
        mModuleHost->Unload();
        unbindProjectResources();
        mProjectFile.reset();
        mDescriptor = {};
        std::lock_guard lock(mMutex);
        mLastError = "Failed to load scene: " + scenePath.string();
        mLogger->LogError("{}", mLastError);
        return false;
    }

    return enterEditor(projectFile, std::move(*loaded), /*loadModule=*/false);
}

void ProjectSession::loadProjectInputBindings(const std::filesystem::path &projectRoot)
{
    if(!mInput)
    {
        return;
    }

    const auto path = projectRoot / "input.json";
    if(!std::filesystem::exists(path))
    {
        mInput->ResetToDefaults();
        return;
    }

    fg::InputMap map;
    std::string error;
    if(!fg::LoadInputMapFile(path, map, &error))
    {
        mLogger->LogWarning("Failed to load input.json ({}): {}", path.string(), error);
        mInput->ResetToDefaults();
        return;
    }

    mInput->LoadBindings(map);
    mLogger->LogInformation("Loaded input bindings from {}", path.string());
}

bool ProjectSession::migrateProjectFile(const std::filesystem::path &projectFile,
                                        ProjectDescriptor &desc, bool force)
{
    const auto migration =
        ProjectMigrator::Migrate(projectFile, desc, DiscoverFriggaRoot(), DiscoverFriggaBuild(),
                                 force);
    if(!migration.ok)
    {
        std::lock_guard lock(mMutex);
        mLastError = migration.error;
        mLogger->LogError("Project migration failed: {}", mLastError);
        return false;
    }

    if(migration.migrated)
    {
        std::lock_guard lock(mMutex);
        mStatusMessage = migration.message;
        mLogger->LogInformation("{}", migration.message);
    }
    return true;
}

bool ProjectSession::MigrateOpenProject(bool force)
{
    if(IsBuilding())
    {
        std::lock_guard lock(mMutex);
        mLastError = "Wait for the module build to finish";
        return false;
    }
    if(!mProjectFile)
    {
        std::lock_guard lock(mMutex);
        mLastError = "No project open";
        return false;
    }

    ProjectDescriptor desc = mDescriptor;
    if(!migrateProjectFile(*mProjectFile, desc, force))
    {
        return false;
    }
    mDescriptor = std::move(desc);
    return true;
}

void ProjectSession::CloseToHome()
{
    if(IsBuilding())
    {
        std::lock_guard lock(mMutex);
        mLastError = "Wait for the module build to finish";
        return;
    }

    if(mSimulation->IsPlaying())
    {
        mSimulation->Stop();
        mSimulation->FlushPending();
    }
    UnloadModule();
    unbindProjectResources();
    clearEditorSessionMarker();
    mPendingStartupScene.reset();
    EditorWindowLayout::RestoreForHome(*mWindow);
    mProjectFile.reset();
    mDescriptor = {};
    mMode       = EditorSessionMode::Home;
    {
        std::lock_guard lock(mMutex);
        mStatusMessage.clear();
    }
    mScene->NewScene();
}

bool ProjectSession::DeleteProject(const std::filesystem::path &projectFileOrRoot)
{
    {
        std::lock_guard lock(mMutex);
        mLastError.clear();
    }

    const auto root = projectRootFromPath(projectFileOrRoot);
    const auto projectFile =
        (projectFileOrRoot.filename() == ProjectFile::FileName ||
         projectFileOrRoot.extension() == ".project")
            ? projectFileOrRoot
            : root / ProjectFile::FileName;

    if(root.empty() || root == root.root_path())
    {
        std::lock_guard lock(mMutex);
        mLastError = "Refusing to delete an invalid project path";
        return false;
    }

    if(mProjectFile)
    {
        std::error_code openEc;
        std::error_code targetEc;
        const auto openRoot =
            std::filesystem::weakly_canonical(mProjectFile->parent_path(), openEc);
        const auto targetRoot = std::filesystem::weakly_canonical(root, targetEc);
        if(!openEc && !targetEc && openRoot == targetRoot)
        {
            std::lock_guard lock(mMutex);
            mLastError = "Close the project before deleting it";
            return false;
        }
    }

    if(!std::filesystem::exists(projectFile) && !std::filesystem::exists(root))
    {
        PreferencesStore::RemoveRecentProject(*mPreferences, projectFile);
        PreferencesStore::RemoveRecentProject(*mPreferences, projectFileOrRoot);
        PreferencesStore::Save(*mPreferences);
        return true;
    }

    if(!std::filesystem::exists(projectFile))
    {
        std::lock_guard lock(mMutex);
        mLastError = "Not a Frigga project (missing frigga.project): " + root.string();
        return false;
    }

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    if(ec)
    {
        std::lock_guard lock(mMutex);
        mLastError = "Failed to delete project: " + ec.message();
        mLogger->LogError("DeleteProject {}: {}", root.string(), mLastError);
        return false;
    }

    PreferencesStore::RemoveRecentProject(*mPreferences, projectFile);
    PreferencesStore::RemoveRecentProject(*mPreferences, projectFileOrRoot);
    PreferencesStore::Save(*mPreferences);
    mLogger->LogInformation("Deleted project {}", root.string());
    return true;
}

std::optional<std::filesystem::path> ProjectSession::GetProjectRoot() const
{
    if(!mProjectFile)
    {
        return std::nullopt;
    }
    return mProjectFile->parent_path();
}

bool ProjectSession::SaveEcsLayout()
{
    const auto root = GetProjectRoot();
    if(!root || !mRegistry)
    {
        return true;
    }

    std::string error;
    if(!fg::SaveEcsLayoutFile(*root / fg::kEcsLayoutFileName, fg::CaptureEcsLayout(*mRegistry),
                              &error))
    {
        std::lock_guard lock(mMutex);
        mLastError = error.empty() ? "Failed to write ecs.json" : error;
        mLogger->LogWarning("Failed to save ecs.json: {}", mLastError);
        return false;
    }
    return true;
}

void ProjectSession::SyncEcsLayout()
{
    const auto root = GetProjectRoot();
    if(!root || !mRegistry)
    {
        return;
    }

    const auto path   = *root / fg::kEcsLayoutFileName;
    const auto result = fg::SyncEcsLayoutFile(*mRegistry, path);
    if(!result.ok)
    {
        mLogger->LogWarning("Failed to sync ecs.json ({}): {}", path.string(), result.error);
        return;
    }
    if(result.addedModuleSystems)
    {
        mLogger->LogInformation("Appended new module system(s) to {} in {}",
                                fg::kDefaultEcsPipelineName, path.string());
    }
}

std::filesystem::path ProjectSession::GetScenesDirectory() const
{
    const auto root = GetProjectRoot();
    if(!root)
    {
        return {};
    }
    return *root / "scenes";
}

std::filesystem::path ProjectSession::GetResourcesDirectory() const
{
    const auto root = GetProjectRoot();
    if(!root)
    {
        return {};
    }
    return *root / ProjectDescriptor::ResourcesDirName;
}

void ProjectSession::bindProjectResources(const std::filesystem::path &projectRoot)
{
    std::string error;
    const auto friggaRoot =
        mDescriptor.friggaRoot.empty() ? DiscoverFriggaRoot() : mDescriptor.friggaRoot;
    if(!ProjectScaffold::EnsureProjectResources(projectRoot, error, friggaRoot))
    {
        mLogger->LogWarning("Project Resources folder: {}", error);
    }
    if(mAssets)
    {
        mAssets->ClearCatalog();
    }
    fg::AssetRegistry::SetResourcesRoot(projectRoot / ProjectDescriptor::ResourcesDirName);
}

void ProjectSession::unbindProjectResources()
{
    if(mAssets)
    {
        mAssets->ClearCatalog();
    }
    fg::AssetRegistry::ResetResourcesRoot();
}

std::vector<std::filesystem::path> ProjectSession::ListSceneFiles() const
{
    std::vector<std::filesystem::path> scenes;
    const auto dir = GetScenesDirectory();
    if(dir.empty() || !std::filesystem::exists(dir))
    {
        return scenes;
    }

    std::error_code ec;
    for(const auto &entry : std::filesystem::directory_iterator(dir, ec))
    {
        if(ec || !entry.is_regular_file())
        {
            continue;
        }
        if(entry.path().extension() == ".json")
        {
            scenes.push_back(entry.path());
        }
    }
    std::sort(scenes.begin(), scenes.end(), [](const auto &a, const auto &b) {
        return a.filename().string() < b.filename().string();
    });
    return scenes;
}

bool ProjectSession::OpenSceneFile(const std::filesystem::path &scenePath)
{
    {
        std::lock_guard lock(mMutex);
        mLastError.clear();
    }
    if(!mProjectFile)
    {
        std::lock_guard lock(mMutex);
        mLastError = "No project open";
        return false;
    }
    if(mSimulation->IsPlaying())
    {
        std::lock_guard lock(mMutex);
        mLastError = "Stop play mode before switching scenes";
        return false;
    }
    if(!std::filesystem::exists(scenePath))
    {
        std::lock_guard lock(mMutex);
        mLastError = "Scene not found: " + scenePath.string();
        return false;
    }
    if(!mScene->LoadScene(scenePath))
    {
        std::lock_guard lock(mMutex);
        mLastError = "Failed to load scene: " + scenePath.string();
        return false;
    }
    {
        std::lock_guard lock(mMutex);
        mStatusMessage = "Opened scene " + scenePath.filename().string();
    }
    return true;
}

bool ProjectSession::SetStartupScene(const std::filesystem::path &scenePath)
{
    {
        std::lock_guard lock(mMutex);
        mLastError.clear();
    }
    if(!mProjectFile)
    {
        std::lock_guard lock(mMutex);
        mLastError = "No project open";
        return false;
    }

    const auto root = mProjectFile->parent_path();
    std::error_code ec;
    const auto relative = std::filesystem::relative(scenePath, root, ec);
    if(ec || relative.empty() || *relative.begin() == "..")
    {
        std::lock_guard lock(mMutex);
        mLastError = "Scene must be inside the project folder";
        return false;
    }

    mDescriptor.sceneRelativePath = relative.generic_string();
    if(!ProjectFile::Save(*mProjectFile, mDescriptor))
    {
        std::lock_guard lock(mMutex);
        mLastError = "Failed to update frigga.project";
        return false;
    }
    {
        std::lock_guard lock(mMutex);
        mStatusMessage = "Startup scene: " + mDescriptor.sceneRelativePath;
    }
    return true;
}

bool ProjectSession::CreateScene(std::string name, fg::SceneTemplate sceneTemplate,
                                 bool setAsStartup)
{
    {
        std::lock_guard lock(mMutex);
        mLastError.clear();
    }
    if(!mProjectFile)
    {
        std::lock_guard lock(mMutex);
        mLastError = "No project open";
        return false;
    }
    if(mSimulation->IsPlaying())
    {
        std::lock_guard lock(mMutex);
        mLastError = "Stop play mode before creating a scene";
        return false;
    }

    // Sanitize to a simple file stem.
    std::string stem;
    stem.reserve(name.size());
    for(const char ch : name)
    {
        if(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-')
        {
            stem.push_back(ch);
        }
        else if(ch == ' ' && !stem.empty() && stem.back() != '_')
        {
            stem.push_back('_');
        }
    }
    while(!stem.empty() && stem.back() == '_')
    {
        stem.pop_back();
    }
    if(stem.empty())
    {
        std::lock_guard lock(mMutex);
        mLastError = "Enter a valid scene name";
        return false;
    }

    const auto scenesDir = GetScenesDirectory();
    std::error_code ec;
    std::filesystem::create_directories(scenesDir, ec);
    auto path = scenesDir / (stem + ".json");
    if(std::filesystem::exists(path))
    {
        std::lock_guard lock(mMutex);
        mLastError = "Scene already exists: " + path.filename().string();
        return false;
    }

    mScene->NewSceneFromTemplate(sceneTemplate);
    if(!mScene->SaveScene(path))
    {
        std::lock_guard lock(mMutex);
        mLastError = "Failed to save scene: " + path.string();
        return false;
    }

    if(setAsStartup && !SetStartupScene(path))
    {
        return false;
    }

    {
        std::lock_guard lock(mMutex);
        mStatusMessage = "Created scene " + path.filename().string();
    }
    return true;
}

bool ProjectSession::BuildModule(std::string cmakeTarget)
{
    if(IsBuilding())
    {
        std::lock_guard lock(mMutex);
        mLastError = "A build is already running";
        return false;
    }
    if(!mProjectFile)
    {
        std::lock_guard lock(mMutex);
        mLastError = "No project open";
        return false;
    }

    joinBuildThread();

    const auto root     = mProjectFile->parent_path();
    const auto buildDir = root / "build";

    mReloadAfterBuild = true;
    mLastOperationWasPublish.store(false, std::memory_order_release);
    mBuildFinished.store(false, std::memory_order_release);
    mBuildExitCode.store(0, std::memory_order_release);
    mBuildPhase.store(ModuleBuildPhase::Configuring, std::memory_order_release);
    mBuildProgress.store(0.05f, std::memory_order_release);
    mBuildProgressDeterminate.store(false, std::memory_order_release);
    mBuildRunning.store(true, std::memory_order_release);
    {
        std::lock_guard lock(mMutex);
        mLastError.clear();
        mBuildLogTail.clear();
        mStatusMessage = cmakeTarget.empty() ? "Building modules…"
                                             : "Building module " + cmakeTarget + "…";
    }

    mLogger->LogInformation("Starting async module build for {} target={}", root.string(),
                            cmakeTarget.empty() ? "(all)" : cmakeTarget);
    mBuildThread =
        std::thread([this, root, buildDir, cmakeTarget = std::move(cmakeTarget)]() {
            runBuildJob(root, buildDir, cmakeTarget);
        });
    return true;
}

bool ProjectSession::PublishGame(const std::filesystem::path &destination)
{
    if(IsBuilding())
    {
        std::lock_guard lock(mMutex);
        mLastError = "A build or publication is already running";
        return false;
    }
    if(!mProjectFile)
    {
        std::lock_guard lock(mMutex);
        mLastError = "No project open";
        return false;
    }
    if(destination.empty())
    {
        std::lock_guard lock(mMutex);
        mLastError = "Publication destination is empty";
        return false;
    }

    std::error_code ec;
    if(std::filesystem::exists(destination, ec) &&
       !std::filesystem::is_empty(destination, ec))
    {
        std::lock_guard lock(mMutex);
        mLastError = "Publication destination must be empty: " + destination.string();
        return false;
    }
    std::filesystem::create_directories(destination, ec);
    if(ec)
    {
        std::lock_guard lock(mMutex);
        mLastError = "Unable to create publication destination: " + ec.message();
        return false;
    }

    joinBuildThread();
    const auto root     = mProjectFile->parent_path();
    const auto buildDir = root / "build";
    mPublishDestination = destination;
    mPublishing.store(true, std::memory_order_release);
    mReloadAfterBuild = false;
    mBuildFinished.store(false, std::memory_order_release);
    mBuildExitCode.store(0, std::memory_order_release);
    mBuildPhase.store(ModuleBuildPhase::Configuring, std::memory_order_release);
    mBuildProgress.store(0.05f, std::memory_order_release);
    mBuildProgressDeterminate.store(false, std::memory_order_release);
    mBuildRunning.store(true, std::memory_order_release);
    {
        std::lock_guard lock(mMutex);
        mLastError.clear();
        mBuildLogTail.clear();
        mStatusMessage = "Publishing game…";
    }

    mBuildThread = std::thread([this, root, buildDir, destination]() {
        runBuildJob(root, buildDir, {}, true, destination);
    });
    return true;
}

void ProjectSession::runBuildJob(std::filesystem::path root, std::filesystem::path buildDir,
                                 std::string cmakeTarget, bool publish,
                                 std::filesystem::path publishDestination)
{
    const auto appendLog = [this](std::string_view line) {
        std::lock_guard lock(mMutex);
        mBuildLogTail.append(line);
        constexpr std::size_t kMaxTail = 4000;
        if(mBuildLogTail.size() > kMaxTail)
        {
            mBuildLogTail.erase(0, mBuildLogTail.size() - kMaxTail);
        }
    };

    ProjectDescriptor engine = mDescriptor;
    if(engine.friggaSdk.empty())
    {
        engine.friggaSdk = DiscoverFriggaSdk();
    }
    if(engine.friggaRoot.empty())
    {
        engine.friggaRoot = DiscoverFriggaRoot();
    }
    if(engine.friggaBuild.empty())
    {
        engine.friggaBuild = DiscoverFriggaBuild();
    }
    FillMissingEnginePaths(engine);
    ProjectScaffold::WriteCMakeUserPresets(root, engine);

    const auto cachePath = FirstExistingCMakeCache({
        engine.friggaBuild,
        engine.friggaSdk,
        engine.friggaBuild.parent_path(),
        engine.friggaSdk.parent_path(),
        ExecutableDirectory(),
    });
    const auto cxxCompiler = ReadCMakeCacheValue(cachePath, "CMAKE_CXX_COMPILER");

    auto appendCachePath = [](std::string &cmd, const char *name, const std::filesystem::path &path) {
        if(path.empty())
        {
            return;
        }
        cmd += " -D";
        cmd += name;
        cmd += "=\"";
        cmd += path.generic_string();
        cmd += "\"";
    };

    const char *buildType = publish ? "Release" : "Debug";
    std::string configureCmd =
        "cmake -S \"" + root.string() + "\" -B \"" + buildDir.string() +
        "\" -G Ninja -DCMAKE_BUILD_TYPE=" + buildType +
        " -DCMAKE_CXX_STANDARD=26"
        " -DCMAKE_CXX_STANDARD_REQUIRED=ON"
        " -DCMAKE_CXX_EXTENSIONS=ON";
    appendCachePath(configureCmd, "FRIGGA_SDK", engine.friggaSdk);
    appendCachePath(configureCmd, "FRIGGA_BUILD", engine.friggaBuild);
    appendCachePath(configureCmd, "FRIGGA_RUNTIME", RuntimeExecutablePath());
    if(!cxxCompiler.empty())
    {
        configureCmd += " -DCMAKE_CXX_COMPILER=\"" + cxxCompiler + "\"";
    }

    mBuildPhase.store(ModuleBuildPhase::Configuring, std::memory_order_release);
    mBuildProgressDeterminate.store(false, std::memory_order_release);
    mBuildProgress.store(0.1f, std::memory_order_release);

    const int configureCode = RunShellCapturing(configureCmd, [&](std::string_view line) {
        appendLog(line);
    });
    if(configureCode != 0)
    {
        mBuildExitCode.store(configureCode, std::memory_order_release);
        mBuildRunning.store(false, std::memory_order_release);
        mBuildFinished.store(true, std::memory_order_release);
        return;
    }

    auto buildCmd = "cmake --build \"" + buildDir.string() + "\"";
    if(!cmakeTarget.empty())
    {
        buildCmd += " --target \"" + cmakeTarget + "\"";
    }
    mBuildPhase.store(ModuleBuildPhase::Building, std::memory_order_release);
    mBuildProgress.store(0.15f, std::memory_order_release);
    mBuildProgressDeterminate.store(false, std::memory_order_release);

    int buildCode = RunShellCapturing(buildCmd, [&](std::string_view line) {
        appendLog(line);
        float fraction = 0.0f;
        if(TryParseNinjaProgress(line, fraction))
        {
            // Map build stage into 0.15 .. 0.9
            const float mapped = 0.15f + fraction * 0.75f;
            mBuildProgress.store(mapped, std::memory_order_release);
            mBuildProgressDeterminate.store(true, std::memory_order_release);
        }
    });

    if(buildCode == 0 && publish)
    {
        const auto staging = root / ".frigga" / "publish-staging";
        std::error_code ec;
        std::filesystem::remove_all(staging, ec);
        std::filesystem::create_directories(staging, ec);
        if(ec)
        {
            appendLog("Unable to create publication staging directory: " + ec.message());
            buildCode = 1;
        }
        else
        {
            const auto installCmd = "cmake --install \"" + buildDir.string() +
                                    "\" --prefix \"" + staging.string() + "\"";
            buildCode = RunShellCapturing(installCmd, [&](std::string_view line) {
                appendLog(line);
            });
        }

        if(buildCode == 0)
        {
            auto publishedDescriptor = mDescriptor;
            publishedDescriptor.friggaSdk.clear();
            publishedDescriptor.friggaRoot.clear();
            publishedDescriptor.friggaBuild.clear();
            if(!ProjectFile::Save(staging / ProjectFile::FileName, publishedDescriptor))
            {
                appendLog("Unable to write sanitized published project manifest");
                buildCode = 1;
            }
        }

        if(buildCode == 0)
        {
            std::filesystem::create_directories(publishDestination, ec);
            for(const auto &entry : std::filesystem::directory_iterator(staging, ec))
            {
                if(ec)
                {
                    break;
                }
                std::filesystem::copy(
                    entry.path(), publishDestination / entry.path().filename(),
                    std::filesystem::copy_options::recursive |
                        std::filesystem::copy_options::overwrite_existing, ec);
                if(ec)
                {
                    break;
                }
            }
            if(ec)
            {
                appendLog("Unable to copy publication: " + ec.message());
                buildCode = 1;
            }
        }
        std::filesystem::remove_all(staging, ec);
    }

    mBuildExitCode.store(buildCode, std::memory_order_release);
    if(buildCode == 0)
    {
        mBuildProgress.store(0.92f, std::memory_order_release);
        mBuildProgressDeterminate.store(true, std::memory_order_release);
    }
    mBuildRunning.store(false, std::memory_order_release);
    mBuildFinished.store(true, std::memory_order_release);
}

bool ProjectSession::ReloadModule()
{
    {
        std::lock_guard lock(mMutex);
        mLastError.clear();
    }
    if(!mProjectFile)
    {
        std::lock_guard lock(mMutex);
        mLastError = "No project open";
        return false;
    }

    if(const auto loaded = ProjectFile::Load(*mProjectFile))
    {
        auto desc = *loaded;
        desc.EnsureGameplayModule();
        mDescriptor = std::move(desc);
    }

    if(!loadEnabledModules())
    {
        std::lock_guard lock(mMutex);
        mLastError     = mModuleHost->GetLastError();
        mStatusMessage = mLastError.empty() ? "No module libraries found — build modules first"
                                            : mLastError;
        return false;
    }

    {
        std::lock_guard lock(mMutex);
        const auto typeIds = mModuleHost->GetRegisteredTypeIds();
        std::string listed;
        for(const auto &id : typeIds)
        {
            if(!listed.empty())
            {
                listed += ", ";
            }
            listed += id;
        }
        mStatusMessage = "Loaded " + std::to_string(mModuleHost->LoadedCount()) +
                         " module(s) | components: " +
                         (listed.empty() ? "(none)" : listed);
    }
    SyncEcsLayout();
    writeEditorSessionMarker();
    return true;
}

void ProjectSession::UnloadModule()
{
    mModuleHost->Unload();
}

void ProjectSession::DismissBuildUi()
{
    if(IsBuilding())
    {
        return;
    }
    mBuildPhase.store(ModuleBuildPhase::Idle, std::memory_order_release);
    mBuildProgress.store(0.0f, std::memory_order_release);
    mBuildProgressDeterminate.store(false, std::memory_order_release);
}

bool ProjectSession::enterEditor(const std::filesystem::path &projectFile, ProjectDescriptor desc,
                                 bool loadModule)
{
    std::string pendingStatus;
    {
        std::lock_guard lock(mMutex);
        pendingStatus = mStatusMessage;
    }

    mProjectFile = projectFile;
    mDescriptor  = std::move(desc);
    mMode        = EditorSessionMode::Editor;
    EditorWindowLayout::PrepareForEditor(*mWindow);
    touchRecent();
    loadProjectInputBindings(projectFile.parent_path());

    std::string opened;
    if(loadModule)
    {
        // Best-effort module load (may be missing until first build).
        if(loadEnabledModules())
        {
            opened = "Opened " + mDescriptor.name;
        }
        else
        {
            opened = "Opened " + mDescriptor.name +
                     " — build modules to load gameplay code";
        }
    }
    else if(mModuleHost->IsLoaded())
    {
        opened = "Opened " + mDescriptor.name;
    }
    else
    {
        const auto lib = moduleLibraryAbsolute();
        if(std::filesystem::exists(lib))
        {
            opened = "Opened " + mDescriptor.name + " (module not loaded)";
        }
        else
        {
            opened = "Opened " + mDescriptor.name + " — build the gameplay module to load code";
        }
    }

    {
        std::lock_guard lock(mMutex);
        if(!pendingStatus.empty() && pendingStatus.find("Migrat") != std::string::npos)
        {
            mStatusMessage = opened + " | " + pendingStatus;
        }
        else
        {
            mStatusMessage = opened;
        }
    }

    writeEditorSessionMarker();
    SyncEcsLayout();
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

std::filesystem::path ProjectSession::moduleLibraryAbsolute() const
{
    ProjectModuleEntry gameplay;
    gameplay.id              = "gameplay";
    gameplay.target          = mDescriptor.moduleTarget.empty() ? "gameplay" : mDescriptor.moduleTarget;
    gameplay.libraryRelative = mDescriptor.moduleLibraryRelative;
    return moduleLibraryAbsolute(gameplay);
}

std::filesystem::path ProjectSession::moduleLibraryAbsolute(const ProjectModuleEntry &entry) const
{
    if(!mProjectFile)
    {
        return {};
    }

    const auto root = mProjectFile->parent_path();
    const auto library =
        entry.libraryRelative.empty() ? ProjectDescriptor::DefaultLibraryRelative(
                                            entry.target.empty() ? entry.id : entry.target)
                                      : entry.libraryRelative;
    const auto configured = root / library;
    std::error_code ec;
    if(std::filesystem::is_regular_file(configured, ec))
    {
        return configured;
    }

    const auto target = entry.target.empty()
                            ? (entry.id.empty() ? std::string("gameplay") : entry.id)
                            : entry.target;
#ifdef _WIN32
    const std::string names[] = {target + ".dll", "lib" + target + ".dll"};
#elif defined(__APPLE__)
    const std::string names[] = {"lib" + target + ".dylib", target + ".dylib"};
#else
    const std::string names[] = {"lib" + target + ".so", target + ".so"};
#endif
    const std::filesystem::path dirs[] = {
        configured.parent_path(),
        root / "build",
        root / "build" / "Debug",
        root / "build" / "Release",
        root / "build" / "RelWithDebInfo",
        root / "build" / "MinSizeRel",
    };
    for(const auto &dir : dirs)
    {
        if(dir.empty())
        {
            continue;
        }
        for(const auto &name : names)
        {
            const auto candidate = dir / name;
            if(std::filesystem::is_regular_file(candidate, ec))
            {
                return candidate;
            }
        }
    }
    return configured;
}

bool ProjectSession::anyEnabledModuleMissing() const
{
    if(!mProjectFile)
    {
        return false;
    }

    ProjectDescriptor desc = mDescriptor;
    desc.EnsureGameplayModule();
    for(const auto &entry : desc.LoadOrder())
    {
        const auto lib = moduleLibraryAbsolute(entry);
        if(!std::filesystem::exists(lib))
        {
            return true;
        }
    }
    return false;
}

bool ProjectSession::tryLoadPendingStartupScene()
{
    if(!mPendingStartupScene)
    {
        return true;
    }

    const auto scenePath = *mPendingStartupScene;
    if(!mScene->LoadScene(scenePath))
    {
        std::lock_guard lock(mMutex);
        mLastError     = "Failed to load scene after module build: " + scenePath.string();
        mStatusMessage = mLastError;
        mLogger->LogError("{}", mLastError);
        return false;
    }

    mPendingStartupScene.reset();
    SyncEcsLayout();
    {
        std::lock_guard lock(mMutex);
        mStatusMessage = "Opened " + mDescriptor.name;
    }
    return true;
}

bool ProjectSession::loadEnabledModules()
{
    mDescriptor.EnsureGameplayModule();
    std::vector<fg::ModuleLoadRequest> requests;
    for(const auto &entry : mDescriptor.LoadOrder())
    {
        const auto lib = moduleLibraryAbsolute(entry);
        if(!std::filesystem::exists(lib))
        {
            continue;
        }
        std::string name = entry.id;
        const auto moduleRoot =
            mProjectFile->parent_path() / ProjectDescriptor::ModulesDirName / entry.id;
        if(const auto manifest = ModuleCatalog::ReadManifest(moduleRoot); manifest &&
                                                                         !manifest->name.empty())
        {
            name = manifest->name;
        }
        requests.push_back(
            fg::ModuleLoadRequest {.id = entry.id, .name = std::move(name), .libraryPath = lib});
    }
    if(requests.empty())
    {
        mModuleHost->Unload();
        return false;
    }
    return mModuleHost->LoadAll(requests);
}

bool ProjectSession::SaveDescriptor()
{
    if(!mProjectFile)
    {
        return false;
    }
    mDescriptor.EnsureGameplayModule();
    return ProjectFile::Save(*mProjectFile, mDescriptor);
}

bool ProjectSession::CreateModule(std::string name)
{
    if(!mProjectFile)
    {
        std::lock_guard lock(mMutex);
        mLastError = "No project open";
        return false;
    }
    std::string error;
    if(!ProjectScaffold::CreateExtraModule(mProjectFile->parent_path(), mDescriptor, std::move(name),
                                           error))
    {
        std::lock_guard lock(mMutex);
        mLastError = error;
        return false;
    }
    if(!SaveDescriptor())
    {
        std::lock_guard lock(mMutex);
        mLastError = "Failed to save frigga.project";
        return false;
    }
    std::lock_guard lock(mMutex);
    mStatusMessage = "Created module " + mDescriptor.modules.back().id;
    return true;
}

bool ProjectSession::InstallModuleFrom(const std::filesystem::path &sourceRoot)
{
    if(!mProjectFile)
    {
        std::lock_guard lock(mMutex);
        mLastError = "No project open";
        return false;
    }
    std::string error;
    if(!ProjectScaffold::InstallModule(mProjectFile->parent_path(), mDescriptor, sourceRoot, error))
    {
        std::lock_guard lock(mMutex);
        mLastError = error;
        return false;
    }
    if(!SaveDescriptor())
    {
        std::lock_guard lock(mMutex);
        mLastError = "Failed to save frigga.project";
        return false;
    }
    std::lock_guard lock(mMutex);
    mStatusMessage = "Installed module from " + sourceRoot.filename().string();
    return true;
}

bool ProjectSession::ExportModule(std::string_view moduleId)
{
    if(!mProjectFile)
    {
        std::lock_guard lock(mMutex);
        mLastError = "No project open";
        return false;
    }
    const auto root = mProjectFile->parent_path();
    const auto source = root / ProjectDescriptor::ModulesDirName / std::string(moduleId);
    if(!std::filesystem::exists(source))
    {
        std::lock_guard lock(mMutex);
        mLastError = "Module folder not found: " + source.string();
        return false;
    }
    const auto dest = EditorPaths::DefaultModulesDir() / std::string(moduleId);
    std::error_code ec;
    if(std::filesystem::exists(dest, ec))
    {
        std::filesystem::remove_all(dest, ec);
    }
    std::string error;
    if(!ModuleCatalog::CopyModuleTree(source, dest, error))
    {
        std::lock_guard lock(mMutex);
        mLastError = error;
        return false;
    }
    std::lock_guard lock(mMutex);
    mStatusMessage = "Exported module to " + dest.string();
    return true;
}

bool ProjectSession::SetModuleEnabled(std::string_view moduleId, bool enabled)
{
    if(!mProjectFile)
    {
        return false;
    }
    bool found = false;
    for(auto &entry : mDescriptor.modules)
    {
        if(entry.id == moduleId)
        {
            entry.enabled = enabled;
            found         = true;
            break;
        }
    }
    if(!found)
    {
        std::lock_guard lock(mMutex);
        mLastError = "Unknown module: " + std::string(moduleId);
        return false;
    }
    if(!SaveDescriptor())
    {
        return false;
    }
    loadEnabledModules();
    SyncEcsLayout();
    return true;
}

std::filesystem::path ProjectSession::projectRootFromPath(
    const std::filesystem::path &projectFileOrRoot)
{
    if(projectFileOrRoot.filename() == ProjectFile::FileName ||
       projectFileOrRoot.extension() == ".project")
    {
        return projectFileOrRoot.parent_path();
    }
    return projectFileOrRoot;
}

bool ProjectSession::OpenInCodeEditor()
{
    if(!mProjectFile)
    {
        std::lock_guard lock(mMutex);
        mLastError = "No project open";
        return false;
    }
    return OpenInCodeEditor(*mProjectFile);
}

bool ProjectSession::OpenInCodeEditor(const std::filesystem::path &projectFileOrRoot)
{
    {
        std::lock_guard lock(mMutex);
        mLastError.clear();
    }

    const auto root = projectRootFromPath(projectFileOrRoot);
    if(root.empty() || !std::filesystem::exists(root))
    {
        std::lock_guard lock(mMutex);
        mLastError = "Project folder not found: " + root.string();
        return false;
    }

    auto command = mPreferences->tools.codeEditorCommand;
    if(command.empty())
    {
        command = "code";
    }

#ifdef _WIN32
    const std::string shell = "start \"\" " + command + " \"" + root.string() + "\"";
    const int         code  = std::system(shell.c_str());
    if(code != 0)
    {
        std::lock_guard lock(mMutex);
        mLastError = "Failed to launch code editor (" + command + ")";
        mLogger->LogError("{}", mLastError);
        return false;
    }
#else
    const auto        folderUtf8 = root.string();
    const std::string shellCmd   = command + " \"" + folderUtf8 + "\"";
    pid_t             pid        = 0;
    char              shBin[]    = "/bin/sh";
    char              shArg[]    = "sh";
    char              dashC[]    = "-c";
    std::string       shellMut   = shellCmd;
    char *const       argv[]     = {shArg, dashC, shellMut.data(), nullptr};
    const int         spawnStatus =
        posix_spawn(&pid, shBin, nullptr, nullptr, argv, environ);
    if(spawnStatus != 0)
    {
        const std::string background = shellCmd + " >/dev/null 2>&1 &";
        const int         code       = std::system(background.c_str());
        if(code != 0)
        {
            std::lock_guard lock(mMutex);
            mLastError = "Failed to launch code editor (" + command + ")";
            mLogger->LogError("{}: spawn={} system={}", mLastError, spawnStatus, code);
            return false;
        }
    }
#endif

    {
        std::lock_guard lock(mMutex);
        mStatusMessage = "Opened in " + command + ": " + root.string();
    }
    mLogger->LogInformation("Launched code editor '{}' on {}", command, root.string());
    return true;
}
