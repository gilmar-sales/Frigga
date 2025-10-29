#pragma once

#include "Editor/Panels/HierarchyLayer.hpp"
#include "Editor/Panels/ResourcesLayer.hpp"
#include "Workflow.hpp"

class GamePlayWorkflow: public Workflow
{
  public:
    GamePlayWorkflow(Ref<HierarchyLayer> hierarchy, Ref<ResourcesLayer> resources)
        : Workflow("GamePlay", {hierarchy, resources})
    {
    }

    ~GamePlayWorkflow() = default;
};
