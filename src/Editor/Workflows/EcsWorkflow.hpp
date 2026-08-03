#pragma once

#include "Editor/Panels/ArchetypesLayer.hpp"
#include "Editor/Panels/HierarchyLayer.hpp"
#include "Editor/Panels/ResourcesLayer.hpp"
#include "Workflow.hpp"

class EcsWorkflow: public Workflow
{
  public:
    EcsWorkflow(skr::Arc<HierarchyLayer> hierarchy, skr::Arc<ArchetypesLayer> archetypes,
                skr::Arc<ResourcesLayer> resources)
        : Workflow("Ecs", {hierarchy, archetypes, resources})
    {
    }

    ~EcsWorkflow() = default;

    void buildDefaultDockLayout(ImGuiID dockspaceId) override;
};
