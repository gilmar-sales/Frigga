#pragma once

#include "../Project/ProjectSession.hpp"

#include <Frigga/Asset/AssetRegistry.hpp>
#include <Frigga/Module/GameplayModuleHost.hpp>
#include <Frigga/Scene/Scene.hpp>
#include <Frigga/Scene/SceneSimulationState.hpp>

#include <Skirnir/Skirnir.hpp>

#include <imgui.h>

/// Bottom strip + expandable background-task panel (JetBrains Rider style).
class StatusBar
{
  public:
    StatusBar(skr::Arc<ProjectSession> session, skr::Arc<fg::Scene> scene,
              skr::Arc<fg::AssetRegistry> assets, skr::Arc<fg::GameplayModuleHost> moduleHost,
              skr::Arc<fg::SceneSimulationState> simulation);

    [[nodiscard]] static float Height();

    /// Draw after the dock host (which must leave `Height()` free at the bottom of WorkSize).
    void Draw(const ImGuiViewport *viewport);

  private:
    void drawStrip(const ImGuiViewport *viewport, float barHeight);
    void drawTasksPanel(const ImGuiViewport *viewport, float barHeight);
    void drawMiniProgress(float width);

    skr::Arc<ProjectSession> mSession;
    skr::Arc<fg::Scene> mScene;
    skr::Arc<fg::AssetRegistry> mAssets;
    skr::Arc<fg::GameplayModuleHost> mModuleHost;
    skr::Arc<fg::SceneSimulationState> mSimulation;

    bool mTasksExpanded = false;
};
