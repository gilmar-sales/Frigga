#pragma once

#include "Editor/Panels/EditorLayer.hpp"
#include "Editor/Panels/GameplayLayer.hpp"
#include "Editor/Panels/HierarchyLayer.hpp"
#include "Editor/Panels/ResourcesLayer.hpp"
#include "Editor/Panels/ScenesLayer.hpp"
#include "Workflow.hpp"

class GamePlayWorkflow: public Workflow
{
  public:
    GamePlayWorkflow(skr::Arc<EditorLayer> editor, skr::Arc<GameplayLayer> gameplay,
                     skr::Arc<HierarchyLayer> hierarchy, skr::Arc<ResourcesLayer> resources,
                     skr::Arc<ScenesLayer> scenes)
        : Workflow("GamePlay", {editor, gameplay, hierarchy, resources, scenes})
    {
    }

    ~GamePlayWorkflow() = default;

    void buildDefaultDockLayout(ImGuiID dockspaceId) override;
};
