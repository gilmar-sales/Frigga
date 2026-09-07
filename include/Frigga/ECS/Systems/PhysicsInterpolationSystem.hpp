#pragma once

#include "Frigga/Physics/IPhysicsWorld.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"

#include <Freyr/Freyr.hpp>

namespace FRIGGA_NAMESPACE
{

    /// Display-rate pose smoothing between fixed physics steps (Main pipeline).
    class PhysicsInterpolationSystem: public fr::System
    {
      public:
        PhysicsInterpolationSystem(const skr::Arc<fr::Registry> &registry,
                                   const skr::Arc<IPhysicsWorld> &physicsWorld,
                                   const skr::Arc<SceneSimulationState> &simulation);
        ~PhysicsInterpolationSystem() override = default;

        void Update(float deltaTime) override;

      private:
        skr::Arc<IPhysicsWorld> mPhysicsWorld;
        skr::Arc<SceneSimulationState> mSimulation;
    };

} // namespace FRIGGA_NAMESPACE
