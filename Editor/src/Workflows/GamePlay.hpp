#pragma once

#include "../Panels/Hierarchy.hpp"
#include "../Panels/Resources.hpp"
#include "../Viewports/Game.hpp"
#include "../Viewports/Scene.hpp"

#include "Workflow.hpp"

class GamePlayLayer: public Workflow
{
  public:
    GamePlayLayer(shared<fg::Scene> scene)
        : Workflow("GamePlay Workflow",
                   {new HierarchyLayer(scene), new GameLayer(scene), new SceneLayer(scene), new ResourcesLayer()})
    {
    }

    ~GamePlayLayer() = default;
};
