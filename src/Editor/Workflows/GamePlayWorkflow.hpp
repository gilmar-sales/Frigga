#pragma once

#include "Editor/Panels/EditorLayer.hpp"
#include "Editor/Panels/GameplayLayer.hpp"
#include "Editor/Panels/HierarchyLayer.hpp"
#include "Editor/Panels/LogsLayer.hpp"
#include "Editor/Panels/ModulesLayer.hpp"
#include "Editor/Panels/ResourcesLayer.hpp"
#include "Editor/Panels/ScenesLayer.hpp"
#include "Workflow.hpp"

class GamePlayWorkflow: public Workflow
{
  public:
    GamePlayWorkflow(skr::Arc<EditorLayer> editor, skr::Arc<GameplayLayer> gameplay,
                     skr::Arc<HierarchyLayer> hierarchy, skr::Arc<ResourcesLayer> resources,
                     skr::Arc<ScenesLayer> scenes, skr::Arc<ModulesLayer> modules,
                     skr::Arc<LogsLayer> logs)
        : Workflow("GamePlay", {editor, gameplay, hierarchy, resources, scenes, modules, logs})
    {
    }

    ~GamePlayWorkflow() = default;

    void buildDefaultDockLayout(ImGuiID dockspaceId) override;
};
