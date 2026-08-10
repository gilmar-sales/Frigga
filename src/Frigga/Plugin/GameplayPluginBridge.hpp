#pragma once

#include "Frigga/Plugin/GameplayPluginHost.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"

#include <Freyr/Freyr.hpp>

namespace FRIGGA_NAMESPACE
{

    /// Fixed Freyr system that forwards Update to the loaded gameplay plugin while playing.
    class GameplayPluginBridge: public fr::System
    {
      public:
        GameplayPluginBridge(const skr::Arc<fr::Registry> &registry,
                             const skr::Arc<GameplayPluginHost> &pluginHost,
                             const skr::Arc<SceneSimulationState> &simulation);
        ~GameplayPluginBridge() override = default;

        void Update(float deltaTime) override;

      private:
        skr::Arc<GameplayPluginHost> mPluginHost;
        skr::Arc<SceneSimulationState> mSimulation;
    };

} // namespace FRIGGA_NAMESPACE
