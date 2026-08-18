#pragma once

#include "Frigga/Net/Network.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"

#include <Freyr/Freyr.hpp>

namespace FRIGGA_NAMESPACE
{

    class NetworkSendSystem: public fr::System
    {
      public:
        NetworkSendSystem(const skr::Arc<fr::Registry> &registry, const skr::Arc<Network> &network,
                          const skr::Arc<SceneSimulationState> &simulation);
        ~NetworkSendSystem() override = default;

        void Update(float deltaTime) override;

      private:
        skr::Arc<Network> mNetwork;
        skr::Arc<SceneSimulationState> mSimulation;
    };

} // namespace FRIGGA_NAMESPACE
