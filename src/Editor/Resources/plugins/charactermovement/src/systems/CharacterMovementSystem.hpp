#pragma once

#include <Frigga/Macro.hpp>
#include <Frigga/Input/Input.hpp>
#include <Frigga/Physics/Physics.hpp>

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

class CharacterMovementSystem: public fr::System
{
  public:
    CharacterMovementSystem(const skr::Arc<fr::Registry> &registry, const skr::Arc<fg::Input> &input,
                            const skr::Arc<fg::Physics> &physics);

    void Update(float deltaTime) override;

  private:
    skr::Arc<fg::Input> mInput;
    skr::Arc<fg::Physics> mPhysics;
};
