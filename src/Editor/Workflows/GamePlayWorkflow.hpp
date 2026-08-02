#pragma once

#include "Editor/Panels/GameplayLayer.hpp"
#include "Editor/Panels/HierarchyLayer.hpp"
#include "Editor/Panels/ResourcesLayer.hpp"
#include "Workflow.hpp"

class GamePlayWorkflow: public Workflow
{
  public:
    GamePlayWorkflow(skr::Arc<GameplayLayer> gameplay, skr::Arc<HierarchyLayer> hierarchy,
                     skr::Arc<ResourcesLayer> resources)
        : Workflow("GamePlay", {gameplay, hierarchy, resources})
    {
    }

    ~GamePlayWorkflow() = default;
};
