#pragma once

#include <Freyr/Freyr.hpp>

namespace FRIGGA_NAMESPACE
{

    class PhysicsSystem: public fr::System
    {
      public:
        PhysicsSystem(const skr::Arc<fr::Registry> &registry): fr::System(registry) {}
        ~PhysicsSystem() override = default;

        void Update(float deltaTime) override;
    };

} // namespace FRIGGA_NAMESPACE
