#include "GameplayPluginBridge.hpp"

namespace FRIGGA_NAMESPACE
{

    GameplayPluginBridge::GameplayPluginBridge(
        const skr::Arc<fr::Registry> &registry,
        const skr::Arc<GameplayPluginHost> &pluginHost,
        const skr::Arc<SceneSimulationState> &simulation)
        : System(registry), mPluginHost(pluginHost), mSimulation(simulation)
    {
    }

    void GameplayPluginBridge::Update(float deltaTime)
    {
        // Continuous play only (single-step while paused does not tick the plugin).
        if(!mSimulation->IsRunning())
        {
            return;
        }

        mPluginHost->UpdatePlugin(deltaTime);
    }

} // namespace FRIGGA_NAMESPACE
