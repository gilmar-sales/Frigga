#pragma once

#include "Editor/SelectionContext.hpp"

#include <Frigga/Frigga.hpp>

class ArchetypesLayer: public fg::Layer
{
  public:
    ArchetypesLayer(skr::Arc<fr::Registry> registry, skr::Arc<SelectionContext> selection);
    ~ArchetypesLayer() override = default;

    void onGui() override;

  private:
    skr::Arc<fr::Registry> mRegistry;
    skr::Arc<SelectionContext> mSelection;
    char mFilter[128] = {};
};
