#pragma once

#include "Editor/Panels/HierarchyLayer.hpp"
#include "Editor/Panels/ResourcesLayer.hpp"
#include "Workflow.hpp"

class EcsWorkflow: public Workflow
{
  public:
    EcsWorkflow(skr::Arc<HierarchyLayer> hierarchy, skr::Arc<ResourcesLayer> resources)
        : Workflow("Ecs", {hierarchy, resources})
    {
    }

    ~EcsWorkflow() = default;
};
