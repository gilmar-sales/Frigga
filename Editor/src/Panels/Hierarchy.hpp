#pragma once

#include <Core/Layer.hpp>
#include <Scene/Scene.hpp>

class HierarchyLayer: public fg::Layer
{
  public:
    HierarchyLayer(shared<fg::Scene> scene);
    ~HierarchyLayer() override = default;

    void createEmptyEntity();
    void drawEntityNode(unsigned entity);

    void drawComponents();
    void addMesh(unsigned entity);

    void onGui() override;

  private:
    shared<fg::Scene> scene;
    unsigned selectionContext;
    unsigned nodeToRename;
};
