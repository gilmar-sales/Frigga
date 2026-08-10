#pragma once

#include "ProjectDescriptor.hpp"
#include "../Preferences/EditorPreferences.hpp"

#include <Frigga/Plugin/GameplayPluginHost.hpp>
#include <Frigga/Scene/Scene.hpp>
#include <Frigga/Scene/SceneSimulationState.hpp>

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

#include <filesystem>
#include <optional>
#include <string>

enum class EditorSessionMode : std::uint8_t
{
    Home = 0,
    Editor,
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
                   skr::Arc<EditorPreferences> preferences,
                   skr::Arc<skr::Logger<ProjectSession>> logger);

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
    [[nodiscard]] const std::string &GetStatusMessage() const
    {
        return mStatusMessage;
    }
    [[nodiscard]] const std::string &GetLastError() const
    {
        return mLastError;
    }

    /// Absolute paths to the Frigga source tree and its build dir (for scaffolds).
    [[nodiscard]] static std::filesystem::path DiscoverFriggaRoot();
    [[nodiscard]] static std::filesystem::path DiscoverFriggaBuild();

    bool CreateProject(const std::filesystem::path &parentDir, std::string name,
                       fg::SceneTemplate sceneTemplate);
    bool OpenProject(const std::filesystem::path &projectFile);
    void CloseToHome();

    bool BuildPlugin();
    bool ReloadPlugin();
    void UnloadPlugin();

  private:
    bool enterEditor(const std::filesystem::path &projectFile, ProjectDescriptor desc);
    void touchRecent();
    [[nodiscard]] std::filesystem::path pluginLibraryAbsolute() const;

    skr::Arc<fg::Scene> mScene;
    skr::Arc<fg::GameplayPluginHost> mPluginHost;
    skr::Arc<fg::SceneSimulationState> mSimulation;
    skr::Arc<EditorPreferences> mPreferences;
    skr::Arc<skr::Logger<ProjectSession>> mLogger;

    EditorSessionMode mMode = EditorSessionMode::Home;
    std::optional<std::filesystem::path> mProjectFile;
    ProjectDescriptor mDescriptor {};
    std::string mStatusMessage;
    std::string mLastError;
};
