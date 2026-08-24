#pragma once

#include <Frigga/Core/LayerStack.hpp>
#include <Frigga/Scene/Scene.hpp>
#include <Frigga/Scene/SceneSimulationState.hpp>

#include "Project/ProjectSession.hpp"
#include "Ui/StatusBar.hpp"

#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

class HierarchyLayer;
class SelectionContext;
class Workflow;

class MainLayer: public fg::Layer
{
  public:
    MainLayer(skr::Arc<fg::Scene> scene, skr::Arc<fg::LayerStack> layerStack,
              skr::Arc<fra::Window> window, skr::Arc<skr::ServiceProvider> serviceProvider,
              skr::Arc<ProjectSession> session);
    ~MainLayer() = default;

    void onUpdate() override;
    void onGui() override;

    float drawTitleBar();
    void drawMenuBar();

  private:
    enum class PendingSceneAction
    {
        None,
        New,
        Open,
        SaveAs,
    };

    void handleShortcuts();
    void processPendingSceneActions();
    void requestNewScene();
    void requestOpenScene();
    void requestSaveScene();
    void requestSaveSceneAs();
    void openSceneDialog();
    void saveSceneDialog();
    void ensureEditMode();
    void ensureGameplayWorkflowDuringPlay();
    void activateWorkflowTab(const char *tabName);

    static void onOpenSceneDialog(void *userdata, const char *const *filelist, int filter);
    static void onSaveSceneDialog(void *userdata, const char *const *filelist, int filter);

    std::vector<std::pair<const char *, skr::Arc<Workflow>>> m_tabIds;
    skr::Arc<Workflow> m_activeTab;
    const char *m_activeTabName = "Gameplay";
    bool m_resetDockLayout      = false;

    skr::Arc<fg::Scene> mScene;
    skr::Arc<fg::LayerStack> mLayerStack;
    skr::Arc<fra::Window> mWindow;
    skr::Arc<HierarchyLayer> mHierarchy;
    skr::Arc<SelectionContext> mSelection;
    skr::Arc<fg::SceneSimulationState> mSimulation;
    skr::Arc<ProjectSession> mSession;
    std::unique_ptr<StatusBar> mStatusBar;

    std::mutex mDialogMutex;
    PendingSceneAction mPendingAction = PendingSceneAction::None;
    std::optional<std::filesystem::path> mPendingPath;
    std::string mDialogDefaultLocation;
};
