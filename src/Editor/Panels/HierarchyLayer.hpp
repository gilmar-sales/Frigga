#pragma once

#include "Frigga/ECS/Components/NameComponent.hpp"

#include <Frigga/Frigga.hpp>

class HierarchyLayer: public fg::Layer
{
  public:
    HierarchyLayer(Ref<fr::Scene> scene);
    ~HierarchyLayer() override = default;

    void createEmptyEntity();
    void drawEntityNode(unsigned entity, fg::NameComponent &name);

    void drawComponents();
    void addMesh(unsigned entity);

    void onGui() override;

  private:
    Ref<fr::Scene> mFreyrScene;
    unsigned selectionContext;
    unsigned nodeToRename;
};