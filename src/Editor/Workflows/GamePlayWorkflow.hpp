#pragma once

#include "Editor/Panels/HierarchyLayer.hpp"
#include "Editor/Panels/ResourcesLayer.hpp"
#include "Workflow.hpp"

class GamePlayWorkflow: public Workflow
{
  public:
    GamePlayWorkflow(skr::Arc<HierarchyLayer> hierarchy, skr::Arc<ResourcesLayer> resources)
        : Workflow("GamePlay", {hierarchy, resources})
    {
    }

    ~GamePlayWorkflow() = default;
};
