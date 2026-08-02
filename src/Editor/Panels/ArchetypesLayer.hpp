#pragma once

#include "Frigga/ECS/Components/NameComponent.hpp"

#include <Frigga/Frigga.hpp>

class ArchetypesLayer: public fg::Layer
{
  public:
    ArchetypesLayer(skr::Arc<fr::Registry> registry);
    ~ArchetypesLayer() override = default;

    void onGui() override;

  private:
    skr::Arc<fr::Registry> mRegistry;
};
