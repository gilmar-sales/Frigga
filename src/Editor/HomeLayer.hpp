#pragma once

#include "Project/ProjectSession.hpp"

#include <Frigga/Core/Layer.hpp>

#include <Freya/Core/Window.hpp>

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>

class HomeLayer: public fg::Layer
{
  public:
    HomeLayer(skr::Arc<ProjectSession> session, skr::Arc<fra::Window> window,
              skr::Arc<EditorPreferences> preferences);

    void onUpdate() override;
    void onGui() override;

  private:
    enum class PendingAction
    {
        None,
        OpenProject,
        BrowseParent,
    };

    void drawNewProjectPanel();
    void drawRecentProjects();
    void requestOpenProjectDialog();
    void requestBrowseParentDialog();
    void processPendingDialogs();

    static void onOpenProjectDialog(void *userdata, const char *const *filelist, int filter);
    static void onBrowseParentDialog(void *userdata, const char *const *filelist, int filter);

    skr::Arc<ProjectSession> mSession;
    skr::Arc<fra::Window> mWindow;
    skr::Arc<EditorPreferences> mPreferences;

    char mProjectName[128] = "MyGame";
    int mTemplateIndex     = 0; // 0 = 3D, 1 = 2D
    char mParentDir[512]   = {};

    std::mutex mDialogMutex;
    PendingAction mPendingAction = PendingAction::None;
    std::optional<std::filesystem::path> mPendingPath;
    std::string mDialogDefaultLocation;
    std::string mUiError;
};
