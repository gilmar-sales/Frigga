#pragma once

#include "Frigga/ECS/Components/NameComponent.hpp"

#include <Frigga/Frigga.hpp>

class HierarchyLayer: public fg::Layer
{
  public:
    HierarchyLayer(skr::Arc<fr::Registry> registry);
    ~HierarchyLayer() override = default;

    void createEmptyEntity();
    void drawEntityNode(fr::Entity entity, fg::NameComponent &name);

    void drawComponents();
    void addMesh(fr::Entity entity);

    void onGui() override;

  private:
    skr::Arc<fr::Registry> mRegistry;
    fr::Entity selectionContext;
    fr::Entity nodeToRename;
};
