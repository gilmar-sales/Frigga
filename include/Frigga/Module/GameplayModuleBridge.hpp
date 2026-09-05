#pragma once

#include "Frigga/Module/GameplayModuleHost.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"

#include <Freyr/Freyr.hpp>

namespace FRIGGA_NAMESPACE
{

    /// Fixed Freyr system that forwards Update to the loaded gameplay module while playing.
    class GameplayModuleBridge: public fr::System
    {
      public:
        GameplayModuleBridge(const skr::Arc<fr::Registry> &registry,
                             const skr::Arc<GameplayModuleHost> &moduleHost,
                             const skr::Arc<SceneSimulationState> &simulation);
        ~GameplayModuleBridge() override = default;

        void Update(float deltaTime) override;

      private:
        skr::Arc<GameplayModuleHost> mModuleHost;
        skr::Arc<SceneSimulationState> mSimulation;
    };

} // namespace FRIGGA_NAMESPACE
