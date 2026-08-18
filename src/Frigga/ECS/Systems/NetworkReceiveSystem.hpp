#pragma once

#include "Frigga/Net/Network.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"

#include <Freyr/Freyr.hpp>

namespace FRIGGA_NAMESPACE
{

    class NetworkReceiveSystem: public fr::System
    {
      public:
        NetworkReceiveSystem(const skr::Arc<fr::Registry> &registry,
                             const skr::Arc<Network> &network,
                             const skr::Arc<SceneSimulationState> &simulation);
        ~NetworkReceiveSystem() override = default;

        void Update(float deltaTime) override;

      private:
        skr::Arc<Network> mNetwork;
        skr::Arc<SceneSimulationState> mSimulation;
    };

} // namespace FRIGGA_NAMESPACE
