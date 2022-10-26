#pragma once

#include <Core/Layer.hpp>

class ResourcesLayer: public fg::Layer
{
  public:
    ResourcesLayer(): fg::Layer("Resources") {}
    ~ResourcesLayer() = default;

    void onGui() override;

    static bool IsOpen;

  private:
};