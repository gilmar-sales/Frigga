#pragma once

#include <Freyr/Freyr.hpp>

namespace FRIGGA_NAMESPACE {

class PhysicsSystem: public fr::System
{
  public:
    PhysicsSystem(Ref<fr::Scene> scene): fr::System(scene) {}
    ~PhysicsSystem() override = default;

    void Update(float deltaTime) override;
};

}
