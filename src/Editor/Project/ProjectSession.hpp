#pragma once

#include "ProjectDescriptor.hpp"
#include "PluginCatalog.hpp"
#include "../Preferences/EditorPreferences.hpp"
#include "../Paths/EditorPaths.hpp"

#include <Frigga/Asset/AssetRegistry.hpp>
#include <Frigga/Input/Input.hpp>
#include <Frigga/Plugin/GameplayPluginHost.hpp>
#include <Frigga/Scene/Scene.hpp>
#include <Frigga/Scene/SceneSimulationState.hpp>

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

#include <atomic>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

enum class EditorSessionMode : std::uint8_t
{
    Home = 0,
    Editor,
};

enum class PluginBuildPhase : std::uint8_t
{
    Idle = 0,
    Configuring,
    Building,
    Reloading,
    Succeeded,
    Failed,
};

enum class EditorBackgroundTaskState : std::uint8_t
{
    Running = 0,
    Succeeded,
    Failed,
};

struct EditorBackgroundTask
{
    std::string id;
    std::string title;
    std::string detail;
    EditorBackgroundTaskState state = EditorBackgroundTaskState::Running;
    float progress                  = 0.0f;
    bool determinate                = false;
    std::string logTail;
};

/**
 * Owns the open Frigga project (if any), engine path discovery, plugin build/load,
 * and Home ↔ Editor mode transitions.
 */
class ProjectSession
{
  public:
    ProjectSession(skr::Arc<fg::Scene> scene,
                   skr::Arc<fg::GameplayPluginHost> pluginHost,
                   skr::Arc<fg::SceneSimulationState> simulation,
                   skr::Arc<fg::Input> input,
                   skr::Arc<fg::AssetRegistry> assets,
                   skr::Arc<fr::Registry> registry,
                   skr::Arc<EditorPreferences> preferences,
                   skr::Arc<skr::Logger<ProjectSession>> logger);
    ~ProjectSession();

    ProjectSession(const ProjectSession &)            = delete;
    ProjectSession &operator=(const ProjectSession &) = delete;

    /// Poll async build completion / trigger plugin reload. Call from the main thread.
    void Poll();

    [[nodiscard]] EditorSessionMode GetMode() const
    {
        return mMode;
    }
    [[nodiscard]] bool IsInEditor() const
    {
        return mMode == EditorSessionMode::Editor;
    }
    [[nodiscard]] bool HasProject() const
    {
        return mProjectFile.has_value();
    }
    [[nodiscard]] const std::optional<std::filesystem::path> &GetProjectFile() const
    {
        return mProjectFile;
    }
    [[nodiscard]] const ProjectDescriptor &GetDescriptor() const
    {
        return mDescriptor;
    }
    [[nodiscard]] std::string GetStatusMessage() const;
    [[nodiscard]] std::string GetLastError() const;

    [[nodiscard]] bool IsBuilding() const
    {
        return mBuildRunning.load(std::memory_order_acquire);
    }
    [[nodiscard]] PluginBuildPhase GetBuildPhase() const;
    /// 0..1 when known from Ninja; otherwise an animated indeterminate value for UI.
    [[nodiscard]] float GetBuildProgress() const;
    [[nodiscard]] bool IsBuildProgressDeterminate() const;
    [[nodiscard]] std::string GetBuildLogTail() const;

    /// Background jobs for the Rider-style status / notifications strip (currently: plugin build).
    [[nodiscard]] std::vector<EditorBackgroundTask> GetBackgroundTasks() const;
    [[nodiscard]] bool HasRunningBackgroundTasks() const;

    /// Absolute paths to the Frigga source tree and its build dir (for scaffolds).
    [[nodiscard]] static std::filesystem::path DiscoverFriggaRoot();
    [[nodiscard]] static std::filesystem::path DiscoverFriggaBuild();
    /// Packaged `Sdk/` next to the Editor when present; otherwise the engine source tree.
    [[nodiscard]] static std::filesystem::path DiscoverFriggaSdk();
    [[nodiscard]] static std::filesystem::path ExecutablePath();

    bool CreateProject(const std::filesystem::path &parentDir, std::string name,
                       fg::SceneTemplate sceneTemplate);
    bool OpenProject(const std::filesystem::path &projectFile);
    /// Deletes the project folder on disk and removes it from recent projects.
    /// Requires a valid frigga.project under the target root. Refuses if that project is open.
    bool DeleteProject(const std::filesystem::path &projectFileOrRoot);
    void CloseToHome();

    [[nodiscard]] std::optional<std::filesystem::path> GetProjectRoot() const;
    [[nodiscard]] std::filesystem::path GetScenesDirectory() const;
    [[nodiscard]] std::filesystem::path GetResourcesDirectory() const;
    /// Absolute paths to `scenes/*.json` in the open project (sorted by filename).
    [[nodiscard]] std::vector<std::filesystem::path> ListSceneFiles() const;
    bool OpenSceneFile(const std::filesystem::path &scenePath);
    /// Creates `scenes/<name>.json` from a template and opens it. Updates startup scene.
    bool CreateScene(std::string name, fg::SceneTemplate sceneTemplate,
                     bool setAsStartup = true);
    bool SetStartupScene(const std::filesystem::path &scenePath);

    /// Starts an asynchronous cmake configure+build. Empty @p cmakeTarget builds all plugins.
    bool BuildPlugin(std::string cmakeTarget = {});
    bool ReloadPlugin();
    void UnloadPlugin();
    void DismissBuildUi();

    bool CreatePlugin(std::string name);
    bool InstallPluginFrom(const std::filesystem::path &sourceRoot);
    bool ExportPlugin(std::string_view pluginId);
    bool SetPluginEnabled(std::string_view pluginId, bool enabled);
    bool SaveDescriptor();

    /// Writes the live pipeline/system layout to `{project}/ecs.json`.
    bool SaveEcsLayout();
    /// Load/apply ecs.json after plugin attach (creates the file on first use).
    void SyncEcsLayout();

    /// Migrates the open project to the current format (or force-rewrites managed files).
    bool MigrateOpenProject(bool force = false);

    /// Opens the current project folder in the preferred code editor (preferences.tools).
    bool OpenInCodeEditor();
    /// Opens any project folder / frigga.project path in the preferred code editor.
    bool OpenInCodeEditor(const std::filesystem::path &projectFileOrRoot);

  private:
    bool enterEditor(const std::filesystem::path &projectFile, ProjectDescriptor desc,
                     bool loadPlugin = true);
    bool migrateProjectFile(const std::filesystem::path &projectFile, ProjectDescriptor &desc,
                            bool force);
    void touchRecent();
    void loadProjectInputBindings(const std::filesystem::path &projectRoot);
    void bindProjectResources(const std::filesystem::path &projectRoot);
    void unbindProjectResources();
    void joinBuildThread();
    void runBuildJob(std::filesystem::path root, std::filesystem::path buildDir,
                     std::string cmakeTarget);
    void writeEditorSessionMarker();
    void clearEditorSessionMarker();
    [[nodiscard]] std::filesystem::path pluginLibraryAbsolute() const;
    [[nodiscard]] std::filesystem::path pluginLibraryAbsolute(const ProjectPluginEntry &entry) const;
    bool loadEnabledPlugins();
    [[nodiscard]] static std::filesystem::path projectRootFromPath(
        const std::filesystem::path &projectFileOrRoot);

    skr::Arc<fg::Scene> mScene;
    skr::Arc<fg::GameplayPluginHost> mPluginHost;
    skr::Arc<fg::SceneSimulationState> mSimulation;
    skr::Arc<fg::Input> mInput;
    skr::Arc<fg::AssetRegistry> mAssets;
    skr::Arc<fr::Registry> mRegistry;
    skr::Arc<EditorPreferences> mPreferences;
    skr::Arc<skr::Logger<ProjectSession>> mLogger;

    EditorSessionMode mMode = EditorSessionMode::Home;
    std::optional<std::filesystem::path> mProjectFile;
    ProjectDescriptor mDescriptor {};

    mutable std::mutex mMutex;
    std::string mStatusMessage;
    std::string mLastError;

    std::atomic<bool> mBuildRunning {false};
    std::atomic<PluginBuildPhase> mBuildPhase {PluginBuildPhase::Idle};
    std::atomic<float> mBuildProgress {0.0f};
    std::atomic<bool> mBuildProgressDeterminate {false};
    std::atomic<bool> mBuildFinished {false};
    std::atomic<int> mBuildExitCode {0};
    std::string mBuildLogTail;
    bool mReloadAfterBuild = false;
    std::thread mBuildThread;
};
