#pragma once

#include "Frigga/Asset/PrimitiveMeshFactory.hpp"
#include "Frigga/ECS/Components/NameComponent.hpp"
#include "Frigga/Scene/Scene.hpp"

#include <Frigga/Frigga.hpp>

class HierarchyLayer: public fg::Layer
{
  public:
    HierarchyLayer(skr::Arc<fr::Registry> registry, skr::Arc<fg::Scene> scene,
                   skr::Arc<fg::PrimitiveMeshFactory> primitives);
    ~HierarchyLayer() override = default;

    void createEmptyEntity();
    void createPrimitiveEntity(fg::PrimitiveType type);
    void createCameraEntity();
    void drawEntityNode(fr::Entity entity, fg::NameComponent &name);

    void drawComponents();

    void onGui() override;

  private:
    [[nodiscard]] bool isEntityLocked(fr::Entity entity) const;
    void setPrimaryCamera(fr::Entity entity);

    skr::Arc<fr::Registry> mRegistry;
    skr::Arc<fg::Scene> mScene;
    skr::Arc<fg::PrimitiveMeshFactory> mPrimitives;
    fr::Entity selectionContext;
    fr::Entity nodeToRename;
};
