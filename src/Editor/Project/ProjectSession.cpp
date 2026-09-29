#include "ProjectSession.hpp"

#include "../EditorWindowLayout.hpp"
#include "../Preferences/PreferencesStore.hpp"
#include "ModuleCatalog.hpp"
#include "ProjectEnginePaths.hpp"
#include "ProjectFile.hpp"
#include "ProjectMigrator.hpp"
#include "ProjectScaffold.hpp"
#include "../Publish/ProcessRunner.hpp"
#include "../Publish/PublishPipeline.hpp"

#include <Frigga/Asset/AssetCooker.hpp>
#include <Frigga/Asset/AssetRegistry.hpp>
#include <Frigga/ECS/EcsLayout.hpp>
#include <Frigga/Graphics/GraphicsConfigIO.hpp>
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
#include <stdio.h>
#include <windows.h>
#elif defined(__APPLE__)
#include <cstdint>
#include <mach-o/dyld.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

namespace
{
    std::filesystem::path ExecutableDirectory()
    {
        const auto path = ProjectSession::ExecutablePath();
        return path.empty() ? std::filesystem::current_path() : path.parent_path();
    }

    bool LooksLikeFriggaRoot(const std::filesystem::path &path)
    {
        return LooksLikeFriggaEngineRoot(path);
    }

    std::string DiscoverCxxCompiler(const ProjectDescriptor &engine)
    {
        return publish::DiscoverCxxCompiler(engine, ExecutableDirectory());
    }

    /// Hash of everything the module configure command depends on. When the
    /// marker in the build dir matches and CMakeCache.txt exists, configure is
    /// skipped and only the build runs.
    std::string ModuleConfigureSignature(const std::filesystem::path &projectRoot,
                                         const std::filesystem::path &sdk,
                                         const std::string &compiler, const std::string &buildType,
                                         bool linkGame)
    {
        const auto manifest  = publish::ReadTextFile(projectRoot / ProjectFile::FileName);
        const auto cmake     = publish::ReadTextFile(projectRoot / "CMakeLists.txt");
        const auto sdkConfig = publish::ReadTextFile(sdk / "FriggaSdkConfig.cmake");
        std::ostringstream signature;
        signature << "buildType=" << buildType << '\n';
        signature << "linkGame=" << (linkGame ? "1" : "0") << '\n';
        signature << "compiler=" << compiler << '\n';
        signature << "sdk=" << sdk.lexically_normal().generic_string() << '\n';
        signature << "sdkConfigHash=" << publish::Fnv1a64Hex(sdkConfig) << '\n';
        signature << "manifestHash=" << publish::Fnv1a64Hex(manifest) << '\n';
        signature << "cmakeHash=" << publish::Fnv1a64Hex(cmake) << '\n';
        return signature.str();
    }
} // namespace

ProjectSession::ProjectSession(skr::Arc<fg::Scene> scene,
                               skr::Arc<fg::GameplayModuleHost> moduleHost,
                               skr::Arc<fg::SceneSimulationState> simulation,
                               skr::Arc<fg::Input> input, skr::Arc<fg::AssetRegistry> assets,
                               skr::Arc<fr::Registry> registry,
                               skr::Arc<EditorPreferences> preferences,
                               skr::Arc<fra::Window> window,
                               skr::Arc<skr::Logger<ProjectSession>> logger,
                               skr::Arc<publish::IProcessRunner> processRunner,
                               skr::Arc<publish::IAssetService> assetService,
                               skr::Arc<publish::PipelineFactory> pipelineFactory)
    : mScene(std::move(scene)), mModuleHost(std::move(moduleHost)),
      mSimulation(std::move(simulation)), mInput(std::move(input)), mAssets(std::move(assets)),
      mRegistry(std::move(registry)), mPreferences(std::move(preferences)),
      mWindow(std::move(window)), mLogger(std::move(logger)),
      mProcessRunner(std::move(processRunner)), mAssetService(std::move(assetService)),
      mPipelineFactory(std::move(pipelineFactory))
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
    using clock = std::chrono::steady_clock;
    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(clock::now().time_since_epoch())
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
    const char *title     = publishing ? "Publish game" : "Build gameplay module";
    task.id               = publishing ? "game-publish" : "gameplay-module-build";
    const auto publishStageLabel = [this](const char *fallback) {
        std::lock_guard lock(mMutex);
        return mPublishStageLabel.empty() ? std::string(fallback) : mPublishStageLabel;
    };

    switch(phase)
    {
    case ModuleBuildPhase::Configuring:
        task.title  = title;
        task.detail = publishing ? publishStageLabel("Configuring (CMake)…")
                                 : "Configuring (CMake)…";
        task.state  = EditorBackgroundTaskState::Running;
        break;
    case ModuleBuildPhase::Building:
        task.title  = title;
        task.detail = publishing ? publishStageLabel("Compiling…") : "Compiling…";
        task.state  = EditorBackgroundTaskState::Running;
        break;
    case ModuleBuildPhase::Reloading:
        task.title  = title;
        task.detail = "Reloading module…";
        task.state  = EditorBackgroundTaskState::Running;
        break;
    case ModuleBuildPhase::Succeeded:
        task.title  = title;
        task.detail = "Succeeded";
        task.state  = EditorBackgroundTaskState::Succeeded;
        break;
    case ModuleBuildPhase::Failed:
        task.title  = title;
        task.detail = GetLastError().empty() ? "Failed" : GetLastError();
        task.state  = EditorBackgroundTaskState::Failed;
        break;
    default:
        return {};
    }

    task.progress    = GetBuildProgress();
    task.determinate = IsBuildProgressDeterminate() || phase == ModuleBuildPhase::Succeeded ||
                       phase == ModuleBuildPhase::Failed;
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
    json << "  \"editorPath\": \"" << publish::EscapeJson(editorPath.generic_string()) << "\",\n";
    json << "  \"soSearchPath\": \"" << publish::EscapeJson(soSearch.generic_string()) << "\",\n";
    json << "  \"moduleLibrary\": \"" << publish::EscapeJson(moduleLib.generic_string()) << "\",\n";
    json << "  \"projectRoot\": \"" << publish::EscapeJson(projectRoot.generic_string()) << "\",\n";
    json << "  \"updatedAt\": \"" << publish::FormatUtcTimestamp() << "\"\n";
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

    const int exitCode       = mBuildExitCode.load(std::memory_order_acquire);
    const bool wasPublishing = mPublishing.exchange(false, std::memory_order_acq_rel);
    mLastOperationWasPublish.store(wasPublishing, std::memory_order_release);
    if(exitCode != 0)
    {
        mBuildPhase.store(ModuleBuildPhase::Failed, std::memory_order_release);
        std::lock_guard lock(mMutex);
        if(wasPublishing && !mPublishError.empty())
        {
            mLastError = "Game publication failed: " + mPublishError;
        }
        else
        {
            mLastError =
                (wasPublishing ? "Game publication failed (exit " : "Module build failed (exit ") +
                std::to_string(exitCode) + ")";
        }
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
       std::filesystem::exists(exeDir / "Editor") || std::filesystem::exists(exeDir / "Editor.exe"))
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

    const auto build                         = DiscoverFriggaBuild();
    const std::filesystem::path candidates[] = {
        build.parent_path(),
        build / "..",
        std::filesystem::current_path(),
        std::filesystem::current_path().parent_path(),
    };
    for(const auto &candidate: candidates)
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

void ProjectSession::applyLocalEnginePaths(ProjectDescriptor &desc)
{
    RefreshEnginePaths(desc, DiscoverFriggaSdk(), DiscoverFriggaRoot(), DiscoverFriggaBuild());
    NormalizeModuleLibraryPaths(desc);
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
    applyLocalEnginePaths(desc);
    desc.moduleLibraryRelative = ProjectDescriptor::DefaultLibraryRelative(desc.moduleTarget);
    const auto result          = ProjectScaffold::Create(parentDir, desc, *mScene);
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

    applyLocalEnginePaths(*loaded);
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

    const bool hadPersistedHostPaths =
        !loaded->friggaSdk.empty() || !loaded->friggaRoot.empty() || !loaded->friggaBuild.empty();
    applyLocalEnginePaths(*loaded);
    if(hadPersistedHostPaths)
    {
        // Drop machine-local engine hints from disk; in-memory paths stay for this session.
        ProjectFile::Save(projectFile, *loaded);
    }

    // Register gameplay types before scene deserialize so userComponents apply immediately.
    mProjectFile = projectFile;
    mDescriptor  = *loaded;

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

void ProjectSession::ensureProjectGraphicsConfig(const std::filesystem::path &projectRoot)
{
    const auto path = projectRoot / fg::kGraphicsConfigFileName;
    std::error_code ec;
    if(std::filesystem::exists(path, ec))
    {
        return;
    }

    fg::GraphicsConfig config = fg::MakeDefaultGraphicsConfig();
    if(mPreferences)
    {
        const auto &g             = mPreferences->graphics;
        const auto &q             = g.gameplayViewport;
        config.width              = g.width;
        config.height             = g.height;
        config.vSync              = g.vSync;
        config.fullscreen         = g.fullscreen;
        config.frameCount         = g.frameCount;
        config.clearColorR        = g.clearColorR;
        config.clearColorG        = g.clearColorG;
        config.clearColorB        = g.clearColorB;
        config.clearColorA        = g.clearColorA;
        config.drawDistance       = g.drawDistance;
        config.maxLights          = g.maxLights;
        config.iblIntensity       = g.iblIntensity;
        config.exposure           = g.exposure;
        config.ambientColorR      = g.ambientColorR;
        config.ambientColorG      = g.ambientColorG;
        config.ambientColorB      = g.ambientColorB;
        config.ambientIntensity   = g.ambientIntensity;
        config.environmentMapPath = g.environmentMapPath;
        config.shaderRoot         = g.shaderRoot;
        config.shadowQuality      = std::string(fg::GraphicsQualityLabel(q.shadowQuality));
        config.ssaoQuality        = std::string(fg::GraphicsQualityLabel(q.ssaoQuality));
        config.taaQuality         = std::string(fg::GraphicsQualityLabel(q.taaQuality));
        config.bloomQuality       = std::string(fg::GraphicsQualityLabel(q.bloomQuality));
        config.ssaoRadius         = g.ssaoRadius;
        config.ssaoBias           = g.ssaoBias;
        config.ssaoPower          = g.ssaoPower;
        config.ssaoIntensity      = g.ssaoIntensity;
        config.deferredDebugView  = g.deferredDebugView;
        config.reverseZ           = g.reverseZ;
        config.animationQuality   = std::string(fg::GraphicsQualityLabel(g.animationQuality));
    }

    std::string error;
    if(!fg::EnsureGraphicsConfigFile(path, config, &error))
    {
        mLogger->LogWarning("Failed to create graphics.json ({}): {}", path.string(),
                            error.empty() ? "unknown error" : error);
        return;
    }
    mLogger->LogInformation("Created graphics.json from current graphics settings");
}

bool ProjectSession::migrateProjectFile(const std::filesystem::path &projectFile,
                                        ProjectDescriptor &desc, bool force)
{
    const auto migration = ProjectMigrator::Migrate(projectFile, desc, DiscoverFriggaRoot(),
                                                    DiscoverFriggaBuild(), force);
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
    applyLocalEnginePaths(desc);
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

    const auto root        = projectRootFromPath(projectFileOrRoot);
    const auto projectFile = (projectFileOrRoot.filename() == ProjectFile::FileName ||
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
    return *root / "Scenes";
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
    auto desc = mDescriptor;
    applyLocalEnginePaths(desc);
    if(!ProjectScaffold::EnsureProjectResources(projectRoot, error, desc.friggaRoot))
    {
        mLogger->LogWarning("Project Resources folder: {}", error);
    }
    if(mAssets)
    {
        mAssets->ClearCatalog();
    }
    fg::AssetRegistry::SetResourcesRoot(projectRoot / ProjectDescriptor::ResourcesDirName);
    if(mAssets)
    {
        mAssets->WarmFonts();
        for(const auto &bankPath: fg::AssetRegistry::DiscoverBankFiles())
        {
            (void)mAssets->LoadBank(bankPath);
        }
    }
}

void ProjectSession::unbindProjectResources()
{
    if(mAssets)
    {
        mAssets->ClearCatalog();
    }
    fg::AssetRegistry::ResetResourcesRoot();
    if(mAssets)
    {
        mAssets->WarmFonts();
    }
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
    for(const auto &entry: std::filesystem::directory_iterator(dir, ec))
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
    for(const char ch: name)
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
        mStatusMessage =
            cmakeTarget.empty() ? "Building modules…" : "Building module " + cmakeTarget + "…";
    }

    mLogger->LogInformation("Starting async module build for {} target={}", root.string(),
                            cmakeTarget.empty() ? "(all)" : cmakeTarget);
    // Resolve host engine paths on the caller thread: the job only sees a copy.
    applyLocalEnginePaths(mDescriptor);
    mBuildThread = std::thread([this, root, buildDir, cmakeTarget = std::move(cmakeTarget),
                                engine = mDescriptor]() {
        runBuildJob(root, buildDir, cmakeTarget, engine);
    });
    return true;
}

bool ProjectSession::PublishGame(const std::filesystem::path &destination, publish::Profile profile,
                                 bool cleanDestination)
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

    joinBuildThread();

    // Resolve host engine paths here, on the caller thread: the job only sees a copy.
    applyLocalEnginePaths(mDescriptor);
    publish::Options options;
    options.projectRoot      = mProjectFile->parent_path();
    options.destination      = destination;
    options.profile          = profile;
    options.cleanDestination = cleanDestination;
    options.descriptor       = mDescriptor;
    options.cxxCompiler      = DiscoverCxxCompiler(mDescriptor);

    mPublishDestination = destination;
    mPublishing.store(true, std::memory_order_release);
    mReloadAfterBuild = false;
    mBuildFinished.store(false, std::memory_order_release);
    mBuildExitCode.store(0, std::memory_order_release);
    mBuildPhase.store(ModuleBuildPhase::Configuring, std::memory_order_release);
    mBuildProgress.store(0.0f, std::memory_order_release);
    mBuildProgressDeterminate.store(false, std::memory_order_release);
    mBuildRunning.store(true, std::memory_order_release);
    {
        std::lock_guard lock(mMutex);
        mLastError.clear();
        mPublishError.clear();
        mBuildLogTail.clear();
        mPublishStageLabel = std::string(publish::StageLabel(publish::Stage::Validate));
        mStatusMessage     = "Publishing game…";
    }

    mLogger->LogInformation("Publishing {} ({}) to {}", options.projectRoot.string(),
                            publish::ToString(profile), destination.string());
    if(!mPipelineFactory)
    {
        std::lock_guard lock(mMutex);
        mLastError     = "Publish pipeline factory not injected (Skirnir wiring missing)";
        mStatusMessage = mLastError;
        mLogger->LogError("{}", mLastError);
        mBuildRunning.store(false, std::memory_order_release);
        mBuildFinished.store(true, std::memory_order_release);
        mBuildExitCode.store(1, std::memory_order_release);
        mBuildPhase.store(ModuleBuildPhase::Failed, std::memory_order_release);
        mPublishing.store(false, std::memory_order_release);
        return false;
    }
    mBuildThread = std::thread([this, options = std::move(options)]() mutable {
        publish::Observer observer;
        observer.onLog      = [this](std::string_view line) { appendBuildLog(line); };
        observer.onProgress = [this](publish::Stage stage, float progress, bool determinate) {
            const bool early = stage == publish::Stage::Validate ||
                               stage == publish::Stage::Configure;
            mBuildPhase.store(early ? ModuleBuildPhase::Configuring : ModuleBuildPhase::Building,
                              std::memory_order_release);
            mBuildProgress.store(progress, std::memory_order_release);
            mBuildProgressDeterminate.store(determinate, std::memory_order_release);
            std::lock_guard lock(mMutex);
            mPublishStageLabel = std::string(publish::StageLabel(stage));
        };

        publish::Pipeline pipeline = mPipelineFactory->Create(std::move(options), std::move(observer));
        const auto report          = pipeline.Run();
        if(!report.ok)
        {
            std::lock_guard lock(mMutex);
            mPublishError = report.error;
            const std::string stage =
                report.failedStage ? std::string(publish::ToString(*report.failedStage)) : "?";
            mLogger->LogError("Publish failed at stage {}: {}", stage, report.error);
        }
        else
        {
            mLogger->LogInformation("Publish finished -> {} ({} files)",
                                    report.destination.string(), report.files.size());
        }
        mBuildExitCode.store(report.ok ? 0 : report.exitCode, std::memory_order_release);
        mBuildRunning.store(false, std::memory_order_release);
        mBuildFinished.store(true, std::memory_order_release);
    });
    return true;
}

void ProjectSession::appendBuildLog(std::string_view line)
{
    std::lock_guard lock(mMutex);
    mBuildLogTail.append(line);
    constexpr std::size_t kMaxTail = 4000;
    if(mBuildLogTail.size() > kMaxTail)
    {
        mBuildLogTail.erase(0, mBuildLogTail.size() - kMaxTail);
    }
}

void ProjectSession::runBuildJob(std::filesystem::path root, std::filesystem::path buildDir,
                                 std::string cmakeTarget, ProjectDescriptor engine)
{
    const auto appendLog = [this](std::string_view line) { appendBuildLog(line); };
    const auto finish    = [this](int code) {
        mBuildExitCode.store(code, std::memory_order_release);
        mBuildRunning.store(false, std::memory_order_release);
        mBuildFinished.store(true, std::memory_order_release);
    };

    if(!mProcessRunner)
    {
        appendLog("Process runner not injected (Skirnir wiring missing)\n");
        mLogger->LogError("Module build failed: process runner not injected");
        finish(1);
        return;
    }
    auto &runner            = *mProcessRunner;
    const auto cxxCompiler = DiscoverCxxCompiler(engine);

    const auto appendCachePath = [](std::string &cmd, const char *name,
                                    const std::filesystem::path &path) {
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

    std::string configureCmd = "cmake -S \"" + root.string() + "\" -B \"" + buildDir.string() +
                               "\" -G Ninja -DCMAKE_BUILD_TYPE=Debug"
                               " -DCMAKE_CXX_STANDARD=26"
                               " -DCMAKE_CXX_STANDARD_REQUIRED=ON"
                               " -DCMAKE_CXX_EXTENSIONS=ON"
                               " -DFRIGGA_MODULES_LINK_GAME=OFF";
    // Always pass SDK paths so CMakeCache is overwritten on host/OS switches.
    appendCachePath(configureCmd, "FRIGGA_SDK", engine.friggaSdk);
    appendCachePath(configureCmd, "FRIGGA_BUILD", engine.friggaBuild);
    if(!cxxCompiler.empty())
    {
        configureCmd += " -DCMAKE_CXX_COMPILER=\"" + cxxCompiler + "\"";
    }

    mBuildPhase.store(ModuleBuildPhase::Configuring, std::memory_order_release);
    mBuildProgressDeterminate.store(false, std::memory_order_release);
    mBuildProgress.store(0.1f, std::memory_order_release);

    // Skip configure when the cache and SDK inputs are unchanged — the common
    // iterative case. The marker is rewritten after every successful configure.
    const auto configureMarker = buildDir / ".frigga-configure-sig";
    const std::string configureSignature =
        ModuleConfigureSignature(root, engine.friggaSdk, cxxCompiler, "Debug", false);
    const bool configureFresh = std::filesystem::exists(buildDir / "CMakeCache.txt") &&
                                std::filesystem::exists(configureMarker) &&
                                publish::ReadTextFile(configureMarker) == configureSignature;
    if(configureFresh)
    {
        appendLog("CMake configure up to date, skipping.\n");
    }
    else
    {
        const int configureCode = runner.Run(configureCmd, appendLog);
        if(configureCode != 0)
        {
            finish(configureCode);
            return;
        }
        std::ofstream marker(configureMarker, std::ios::binary | std::ios::trunc);
        if(marker)
        {
            marker << configureSignature;
        }
    }

    auto buildCmd = "cmake --build \"" + buildDir.string() + "\" --parallel";
    if(!cmakeTarget.empty())
    {
        buildCmd += " --target \"" + cmakeTarget + "\"";
    }
    mBuildPhase.store(ModuleBuildPhase::Building, std::memory_order_release);
    mBuildProgress.store(0.15f, std::memory_order_release);
    mBuildProgressDeterminate.store(false, std::memory_order_release);

    const int buildCode = runner.Run(buildCmd, [&](std::string_view line) {
        appendLog(line);
        float fraction = 0.0f;
        if(publish::TryParseNinjaProgress(line, fraction))
        {
            // Map build stage into 0.15 .. 0.9
            mBuildProgress.store(0.15f + fraction * 0.75f, std::memory_order_release);
            mBuildProgressDeterminate.store(true, std::memory_order_release);
        }
    });

    if(buildCode == 0)
    {
        mBuildProgress.store(0.92f, std::memory_order_release);
        mBuildProgressDeterminate.store(true, std::memory_order_release);
    }
    finish(buildCode);
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
        auto desc   = *loaded;
        mDescriptor = std::move(desc);
    }

    if(!loadEnabledModules())
    {
        std::lock_guard lock(mMutex);
        mLastError = mModuleHost->GetLastError();
        mStatusMessage =
            mLastError.empty() ? "No module libraries found — build modules first" : mLastError;
        return false;
    }

    {
        std::lock_guard lock(mMutex);
        const auto typeIds = mModuleHost->GetRegisteredTypeIds();
        std::string listed;
        for(const auto &id: typeIds)
        {
            if(!listed.empty())
            {
                listed += ", ";
            }
            listed += id;
        }
        mStatusMessage = "Loaded " + std::to_string(mModuleHost->LoadedCount()) +
                         " module(s) | components: " + (listed.empty() ? "(none)" : listed);
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
    ensureProjectGraphicsConfig(projectFile.parent_path());

    {
        std::string mcpError;
        if(!ProjectScaffold::EnsureCursorMcp(projectFile.parent_path(), mDescriptor, mcpError))
        {
            mLogger->LogWarning("Cursor MCP setup skipped: {}", mcpError);
        }
    }

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
            opened = "Opened " + mDescriptor.name + " — build modules to load gameplay code";
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
    for(const auto &entry: mDescriptor.modules)
    {
        if(entry.IsGameplay())
        {
            return moduleLibraryAbsolute(entry);
        }
    }
    return {};
}

std::filesystem::path ProjectSession::moduleLibraryAbsolute(const ProjectModuleEntry &entry) const
{
    if(!mProjectFile)
    {
        return {};
    }

    const auto root       = mProjectFile->parent_path();
    const auto library    = entry.libraryRelative.empty()
                                ? ProjectDescriptor::DefaultLibraryRelative(
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
        configured.parent_path(),          root / "build",
        root / "build" / "Debug",          root / "build" / "Release",
        root / "build" / "RelWithDebInfo", root / "build" / "MinSizeRel",
    };
    for(const auto &dir: dirs)
    {
        if(dir.empty())
        {
            continue;
        }
        for(const auto &name: names)
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
    for(const auto &entry: desc.LoadOrder())
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
    std::vector<fg::ModuleLoadRequest> requests;
    for(const auto &entry: mDescriptor.LoadOrder())
    {
        const auto lib = moduleLibraryAbsolute(entry);
        if(!std::filesystem::exists(lib))
        {
            continue;
        }
        std::string name = entry.id;
        const auto moduleRoot =
            mProjectFile->parent_path() / ProjectDescriptor::ModulesDirName / entry.id;
        if(const auto manifest = ModuleCatalog::ReadManifest(moduleRoot);
           manifest && !manifest->name.empty())
        {
            name = manifest->name;
        }
        requests.push_back(
            fg::ModuleLoadRequest{.id = entry.id, .name = std::move(name), .libraryPath = lib});
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
    if(!ProjectScaffold::CreateExtraModule(mProjectFile->parent_path(), mDescriptor,
                                           std::move(name), error))
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
    const auto root   = mProjectFile->parent_path();
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
    for(auto &entry: mDescriptor.modules)
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
    const int code          = std::system(shell.c_str());
    if(code != 0)
    {
        std::lock_guard lock(mMutex);
        mLastError = "Failed to launch code editor (" + command + ")";
        mLogger->LogError("{}", mLastError);
        return false;
    }
#else
    const auto folderUtf8      = root.string();
    const std::string shellCmd = command + " \"" + folderUtf8 + "\"";
    pid_t pid                  = 0;
    char shBin[]               = "/bin/sh";
    char shArg[]               = "sh";
    char dashC[]               = "-c";
    std::string shellMut       = shellCmd;
    char *const argv[]         = {shArg, dashC, shellMut.data(), nullptr};
    const int spawnStatus      = posix_spawn(&pid, shBin, nullptr, nullptr, argv, environ);
    if(spawnStatus != 0)
    {
        const std::string background = shellCmd + " >/dev/null 2>&1 &";
        const int code               = std::system(background.c_str());
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
