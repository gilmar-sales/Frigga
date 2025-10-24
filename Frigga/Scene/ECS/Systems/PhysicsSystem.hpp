#pragma once

#include <Freyr/Freyr.hpp>

FRIGGA_BEGIN

class PhysicsSystem: public fr::System
{
  public:
    PhysicsSystem(Ref<fr::Scene> scene): fr::System(scene) {}
    ~PhysicsSystem() = default;

    void Update(float deltaTime) override;
};

FRIGGA_END
