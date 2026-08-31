#pragma once

#include "../Project/ProjectSession.hpp"

#include <Frigga/Core/Layer.hpp>
#include <Frigga/Scene/Scene.hpp>
#include <Frigga/Scene/SceneSimulationState.hpp>

#include <filesystem>
#include <string>
#include <vector>

class ScenesLayer: public fg::Layer
{
  public:
    ScenesLayer(skr::Arc<ProjectSession> session, skr::Arc<fg::Scene> scene,
                skr::Arc<fg::SceneSimulationState> simulation);
    ~ScenesLayer() override = default;

    void onUpdate() override;
    void onGui() override;

  private:
    void refresh();
    void drawToolbar();
    void drawList();

    skr::Arc<ProjectSession> mSession;
    skr::Arc<fg::Scene> mScene;
    skr::Arc<fg::SceneSimulationState> mSimulation;

    std::vector<std::filesystem::path> mScenes;
    char mNewName[128] = "NewScene";
    int mTemplateIndex = 0;
    std::string mStatus;
};
