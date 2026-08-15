#pragma once

#include "../Project/ProjectSession.hpp"
#include "../Project/PluginCatalog.hpp"

#include <Frigga/Core/Layer.hpp>
#include <Frigga/Plugin/GameplayPluginHost.hpp>
#include <Frigga/Scene/SceneSimulationState.hpp>

#include <string>
#include <vector>

class PluginsLayer: public fg::Layer
{
  public:
    PluginsLayer(skr::Arc<ProjectSession> session, skr::Arc<fg::GameplayPluginHost> pluginHost,
                 skr::Arc<fg::SceneSimulationState> simulation);
    ~PluginsLayer() override = default;

    void onUpdate() override;
    void onGui() override;

  private:
    void refreshLibrary();
    void drawProjectPlugins();
    void drawLibrary();

    skr::Arc<ProjectSession> mSession;
    skr::Arc<fg::GameplayPluginHost> mPluginHost;
    skr::Arc<fg::SceneSimulationState> mSimulation;

    char mNewName[128] = "Combat";
    std::string mStatus;
    std::vector<DiscoveredPlugin> mUserLibrary;
    std::vector<DiscoveredPlugin> mBundled;
};
