#pragma once

#include <Frigga/Macro.hpp>
#include <Frigga/Input/Input.hpp>

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

class ThirdPersonCameraSystem: public fr::System
{
  public:
    ThirdPersonCameraSystem(const skr::Arc<fr::Registry> &registry, const skr::Arc<fg::Input> &input);
    ~ThirdPersonCameraSystem() override = default;

    void Update(float deltaTime) override;

  private:
    skr::Arc<fg::Input> mInput;
};
