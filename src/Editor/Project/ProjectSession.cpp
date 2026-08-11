#include "ProjectSession.hpp"

#include "../Preferences/PreferencesStore.hpp"
#include "ProjectFile.hpp"
#include "ProjectMigrator.hpp"
#include "ProjectScaffold.hpp"

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

    bool LooksLikeFriggaSdk(const std::filesystem::path &path)
    {
        return std::filesystem::exists(path / "src/Frigga/Plugin/frigga_plugin.h") &&
               std::filesystem::exists(path / "_deps/freyr-src/include/Freyr");
    }

    bool LooksLikeFriggaRoot(const std::filesystem::path &path)
    {
        if(LooksLikeFriggaSdk(path))
        {
            return true;
        }
        return std::filesystem::exists(path / "src/Frigga/Frigga.hpp") &&
               std::filesystem::exists(path / "CMakeLists.txt");
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
} // namespace

ProjectSession::ProjectSession(skr::Arc<fg::Scene> scene,
                               skr::Arc<fg::GameplayPluginHost> pluginHost,
                               skr::Arc<fg::SceneSimulationState> simulation,
                               skr::Arc<fg::Input> input,
                               skr::Arc<EditorPreferences> preferences,
                               skr::Arc<skr::Logger<ProjectSession>> logger)
    : mScene(std::move(scene)), mPluginHost(std::move(pluginHost)),
      mSimulation(std::move(simulation)), mInput(std::move(input)),
      mPreferences(std::move(preferences)), mLogger(std::move(logger))
{
}

ProjectSession::~ProjectSession()
{
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

PluginBuildPhase ProjectSession::GetBuildPhase() const
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
    if(phase == PluginBuildPhase::Idle)
    {
        return {};
    }

    EditorBackgroundTask task;
    task.id = "gameplay-plugin-build";

    switch(phase)
    {
    case PluginBuildPhase::Configuring:
        task.title  = "Build gameplay plugin";
        task.detail = "Configuring (CMake)…";
        task.state   = EditorBackgroundTaskState::Running;
        break;
    case PluginBuildPhase::Building:
        task.title  = "Build gameplay plugin";
        task.detail = "Compiling…";
        task.state   = EditorBackgroundTaskState::Running;
        break;
    case PluginBuildPhase::Reloading:
        task.title  = "Build gameplay plugin";
        task.detail = "Reloading plugin…";
        task.state   = EditorBackgroundTaskState::Running;
        break;
    case PluginBuildPhase::Succeeded:
        task.title  = "Build gameplay plugin";
        task.detail = "Succeeded";
        task.state   = EditorBackgroundTaskState::Succeeded;
        break;
    case PluginBuildPhase::Failed:
        task.title  = "Build gameplay plugin";
        task.detail = GetLastError().empty() ? "Failed" : GetLastError();
        task.state   = EditorBackgroundTaskState::Failed;
        break;
    default:
        return {};
    }

    task.progress    = GetBuildProgress();
    task.determinate = IsBuildProgressDeterminate() ||
                       phase == PluginBuildPhase::Succeeded || phase == PluginBuildPhase::Failed;
    if(phase == PluginBuildPhase::Succeeded || phase == PluginBuildPhase::Failed)
    {
        task.progress = 1.0f;
    }
    task.logTail = GetBuildLogTail();
    return {std::move(task)};
}

bool ProjectSession::HasRunningBackgroundTasks() const
{
    const auto phase = GetBuildPhase();
    return IsBuilding() || phase == PluginBuildPhase::Reloading;
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
    const auto pluginLib  = pluginLibraryAbsolute().lexically_normal();

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
    json << "  \"pluginLibrary\": \"" << EscapeJson(pluginLib.generic_string()) << "\",\n";
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
    if(exitCode != 0)
    {
        mBuildPhase.store(PluginBuildPhase::Failed, std::memory_order_release);
        std::lock_guard lock(mMutex);
        mLastError     = "Plugin build failed (exit " + std::to_string(exitCode) + ")";
        mStatusMessage = mLastError;
        mLogger->LogError("{}", mLastError);
        return;
    }

    if(mReloadAfterBuild)
    {
        mBuildPhase.store(PluginBuildPhase::Reloading, std::memory_order_release);
        mBuildProgress.store(0.95f, std::memory_order_release);
        mBuildProgressDeterminate.store(true, std::memory_order_release);

        if(ReloadPlugin())
        {
            mBuildPhase.store(PluginBuildPhase::Succeeded, std::memory_order_release);
            mBuildProgress.store(1.0f, std::memory_order_release);
            // ReloadPlugin already set mStatusMessage with the registered type list.
        }
        else
        {
            mBuildPhase.store(PluginBuildPhase::Failed, std::memory_order_release);
        }
    }
    else
    {
        mBuildPhase.store(PluginBuildPhase::Succeeded, std::memory_order_release);
        mBuildProgress.store(1.0f, std::memory_order_release);
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

    return enterEditor(result.projectFile, std::move(*loaded));
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
    // If the .so is missing, deserialize stashes them until the first successful plugin load.
    mProjectFile = projectFile;
    mDescriptor  = *loaded;
    const auto lib = pluginLibraryAbsolute();
    if(std::filesystem::exists(lib))
    {
        if(!mPluginHost->Load(lib))
        {
            mLogger->LogWarning("Plugin present but failed to load before scene: {}",
                                mPluginHost->GetLastError());
        }
    }

    const auto root      = projectFile.parent_path();
    const auto scenePath = root / loaded->sceneRelativePath;
    if(!mScene->LoadScene(scenePath))
    {
        mPluginHost->Unload();
        mProjectFile.reset();
        mDescriptor = {};
        std::lock_guard lock(mMutex);
        mLastError = "Failed to load scene: " + scenePath.string();
        mLogger->LogError("{}", mLastError);
        return false;
    }

    return enterEditor(projectFile, std::move(*loaded), /*loadPlugin=*/false);
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
        mLastError = "Wait for the plugin build to finish";
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
        mLastError = "Wait for the plugin build to finish";
        return;
    }

    if(mSimulation->IsPlaying())
    {
        mSimulation->Stop();
    }
    UnloadPlugin();
    clearEditorSessionMarker();
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

std::filesystem::path ProjectSession::GetScenesDirectory() const
{
    const auto root = GetProjectRoot();
    if(!root)
    {
        return {};
    }
    return *root / "scenes";
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

bool ProjectSession::BuildPlugin()
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
    mBuildFinished.store(false, std::memory_order_release);
    mBuildExitCode.store(0, std::memory_order_release);
    mBuildPhase.store(PluginBuildPhase::Configuring, std::memory_order_release);
    mBuildProgress.store(0.05f, std::memory_order_release);
    mBuildProgressDeterminate.store(false, std::memory_order_release);
    mBuildRunning.store(true, std::memory_order_release);
    {
        std::lock_guard lock(mMutex);
        mLastError.clear();
        mBuildLogTail.clear();
        mStatusMessage = "Building gameplay plugin…";
    }

    mLogger->LogInformation("Starting async plugin build for {}", root.string());
    mBuildThread = std::thread([this, root, buildDir]() { runBuildJob(root, buildDir); });
    return true;
}

void ProjectSession::runBuildJob(std::filesystem::path root, std::filesystem::path buildDir)
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

    const auto cachePath =
        (!mDescriptor.friggaBuild.empty() ? mDescriptor.friggaBuild : DiscoverFriggaBuild()) /
        "CMakeCache.txt";
    const auto cxxCompiler = ReadCMakeCacheValue(cachePath, "CMAKE_CXX_COMPILER");

    std::string configureCmd =
        "cmake -S \"" + root.string() + "\" -B \"" + buildDir.string() +
        "\" -G Ninja -DCMAKE_BUILD_TYPE=Debug"
        " -DCMAKE_CXX_STANDARD=26"
        " -DCMAKE_CXX_STANDARD_REQUIRED=ON"
        " -DCMAKE_CXX_EXTENSIONS=ON";
    if(!cxxCompiler.empty())
    {
        configureCmd += " -DCMAKE_CXX_COMPILER=\"" + cxxCompiler + "\"";
    }

    mBuildPhase.store(PluginBuildPhase::Configuring, std::memory_order_release);
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

    const auto buildCmd = "cmake --build \"" + buildDir.string() + "\"";
    mBuildPhase.store(PluginBuildPhase::Building, std::memory_order_release);
    mBuildProgress.store(0.15f, std::memory_order_release);
    mBuildProgressDeterminate.store(false, std::memory_order_release);

    const int buildCode = RunShellCapturing(buildCmd, [&](std::string_view line) {
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

    mBuildExitCode.store(buildCode, std::memory_order_release);
    if(buildCode == 0)
    {
        mBuildProgress.store(0.92f, std::memory_order_release);
        mBuildProgressDeterminate.store(true, std::memory_order_release);
    }
    mBuildRunning.store(false, std::memory_order_release);
    mBuildFinished.store(true, std::memory_order_release);
}

bool ProjectSession::ReloadPlugin()
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

    const auto lib = pluginLibraryAbsolute();
    if(!mPluginHost->Load(lib))
    {
        std::lock_guard lock(mMutex);
        mLastError     = mPluginHost->GetLastError();
        mStatusMessage = mLastError;
        return false;
    }

    {
        std::lock_guard lock(mMutex);
        const auto typeIds = mPluginHost->GetRegisteredTypeIds();
        std::string listed;
        for(const auto &id : typeIds)
        {
            if(!listed.empty())
            {
                listed += ", ";
            }
            listed += id;
        }
        mStatusMessage = "Loaded plugin " + lib.filename().string() + " | components: " +
                         (listed.empty() ? "(none)" : listed);
    }
    writeEditorSessionMarker();
    return true;
}

void ProjectSession::UnloadPlugin()
{
    mPluginHost->Unload();
}

void ProjectSession::DismissBuildUi()
{
    if(IsBuilding())
    {
        return;
    }
    mBuildPhase.store(PluginBuildPhase::Idle, std::memory_order_release);
    mBuildProgress.store(0.0f, std::memory_order_release);
    mBuildProgressDeterminate.store(false, std::memory_order_release);
}

bool ProjectSession::enterEditor(const std::filesystem::path &projectFile, ProjectDescriptor desc,
                                 bool loadPlugin)
{
    std::string pendingStatus;
    {
        std::lock_guard lock(mMutex);
        pendingStatus = mStatusMessage;
    }

    mProjectFile = projectFile;
    mDescriptor  = std::move(desc);
    mMode        = EditorSessionMode::Editor;
    touchRecent();
    loadProjectInputBindings(projectFile.parent_path());

    std::string opened;
    if(loadPlugin)
    {
        // Best-effort plugin load (may be missing until first build).
        const auto lib = pluginLibraryAbsolute();
        if(std::filesystem::exists(lib))
        {
            if(mPluginHost->Load(lib))
            {
                opened = "Opened " + mDescriptor.name;
            }
            else
            {
                opened = "Opened " + mDescriptor.name + " (plugin not loaded)";
            }
        }
        else
        {
            opened = "Opened " + mDescriptor.name + " — build the gameplay plugin to load code";
        }
    }
    else if(mPluginHost->IsLoaded())
    {
        opened = "Opened " + mDescriptor.name;
    }
    else
    {
        const auto lib = pluginLibraryAbsolute();
        if(std::filesystem::exists(lib))
        {
            opened = "Opened " + mDescriptor.name + " (plugin not loaded)";
        }
        else
        {
            opened = "Opened " + mDescriptor.name + " — build the gameplay plugin to load code";
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
