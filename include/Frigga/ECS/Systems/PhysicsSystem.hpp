#pragma once

#include "Frigga/Physics/IPhysicsWorld.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"

#include <Freyr/Freyr.hpp>

namespace FRIGGA_NAMESPACE
{

    struct TransformComponent;

    /// Writes a simulated world pose into the local transform (keeps world scale). Only reads
    /// other entities, so it is safe from EachAsync; call MarkDynamicBodiesDirty afterwards.
    void WriteBodyPose(fr::Registry &registry, fr::Entity entity, TransformComponent &transform,
                       const glm::vec3 &position, const glm::quat &rotation);
    /// Marks every dynamic body written back by physics as hierarchy-dirty (single thread).
    void MarkDynamicBodiesDirty(fr::Registry &registry);

    class PhysicsSystem: public fr::System
    {
      public:
        PhysicsSystem(const skr::Arc<fr::Registry> &registry,
                      const skr::Arc<IPhysicsWorld> &physicsWorld,
                      const skr::Arc<SceneSimulationState> &simulation);
        ~PhysicsSystem() override = default;

        void PreUpdate(float deltaTime) override;
        void Update(float deltaTime) override;

      private:
        skr::Arc<IPhysicsWorld> mPhysicsWorld;
        skr::Arc<SceneSimulationState> mSimulation;
        bool mStepOnce = false;
    };

} // namespace FRIGGA_NAMESPACE
