#include "GameplayModuleBridge.hpp"

namespace FRIGGA_NAMESPACE
{

    GameplayModuleBridge::GameplayModuleBridge(
        const skr::Arc<fr::Registry> &registry,
        const skr::Arc<GameplayModuleHost> &moduleHost,
        const skr::Arc<SceneSimulationState> &simulation)
        : System(registry), mModuleHost(moduleHost), mSimulation(simulation)
    {
    }

    void GameplayModuleBridge::Update(float deltaTime)
    {
        // Continuous play only (single-step while paused does not tick the module).
        if(!mSimulation->IsRunning())
        {
            return;
        }

        mModuleHost->UpdateModule(deltaTime);
    }

} // namespace FRIGGA_NAMESPACE
