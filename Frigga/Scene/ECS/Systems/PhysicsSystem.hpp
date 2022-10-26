#pragma once

#include <Freyr/Core/BaseSystem.hpp>

#include "../Design.hpp"

FRIGGA_BEGIN

class PhysicsSystem : public freyr::BaseSystem
{
  public:
    using Signature = std::tuple<TransformComponent>;

    PhysicsSystem()  = default;
    ~PhysicsSystem() = default;

    void onUpdate() override;
};

FRIGGA_END
