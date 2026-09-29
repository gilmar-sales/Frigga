#pragma once

#include <Frigga/Network/NetworkClient.hpp>
#include <Frigga/Network/NetworkServer.hpp>
#include <Frigga/Scene/SceneSimulationState.hpp>

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

namespace FRIGGA_NAMESPACE
{

    /// Ticks the authoritative server inside the Simulation pipeline:
    /// Poll (receive inputs/RPCs) then broadcast snapshots at snapshotHz.
    class NetworkServerSystem: public fr::System
    {
      public:
        NetworkServerSystem(const skr::Arc<fr::Registry> &registry,
                            const skr::Arc<NetworkServer> &server,
                            const skr::Arc<SceneSimulationState> &simulation);
        ~NetworkServerSystem() override = default;

        void Update(float deltaTime) override;

      private:
        skr::Arc<NetworkServer> mServer;
        skr::Arc<SceneSimulationState> mSimulation;
    };

    /// Ticks the client: Poll (buffer snapshots, apply spawn/despawn) then
    /// interpolate transforms. Runs in Simulation so client ghosts step at a
    /// fixed rate; rendering reads the interpolated pose.
    class NetworkClientSystem: public fr::System
    {
      public:
        NetworkClientSystem(const skr::Arc<fr::Registry> &registry,
                            const skr::Arc<NetworkClient> &client,
                            const skr::Arc<SceneSimulationState> &simulation);
        ~NetworkClientSystem() override = default;

        void Update(float deltaTime) override;

      private:
        skr::Arc<NetworkClient> mClient;
        skr::Arc<SceneSimulationState> mSimulation;
        double mClock = 0.0;
    };

} // namespace FRIGGA_NAMESPACE
