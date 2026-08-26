#pragma once

#include "../Project/ProjectSession.hpp"
#include "../Project/ModuleCatalog.hpp"

#include <Frigga/Core/Layer.hpp>
#include <Frigga/Module/GameplayModuleHost.hpp>
#include <Frigga/Scene/SceneSimulationState.hpp>

#include <string>
#include <vector>

class ModulesLayer: public fg::Layer
{
  public:
    ModulesLayer(skr::Arc<ProjectSession> session, skr::Arc<fg::GameplayModuleHost> moduleHost,
                 skr::Arc<fg::SceneSimulationState> simulation);
    ~ModulesLayer() override = default;

    void onUpdate() override;
    void onGui() override;

  private:
    void refreshLibrary();
    void drawProjectModules();
    void drawLibrary();

    skr::Arc<ProjectSession> mSession;
    skr::Arc<fg::GameplayModuleHost> mModuleHost;
    skr::Arc<fg::SceneSimulationState> mSimulation;

    char mNewName[128] = "Combat";
    std::string mStatus;
    std::vector<DiscoveredModule> mUserLibrary;
    std::vector<DiscoveredModule> mBundled;
};
