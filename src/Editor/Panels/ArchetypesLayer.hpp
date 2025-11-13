#pragma once

#include "Frigga/ECS/Components/NameComponent.hpp"

#include <Frigga/Frigga.hpp>

class ArchetypesLayer: public fg::Layer
{
  public:
    ArchetypesLayer(Ref<fr::Scene> scene);
    ~ArchetypesLayer() override = default;

    void onGui() override;

  private:
    Ref<fr::Scene> mFreyrScene;
};