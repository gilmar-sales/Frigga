#pragma once

#include "Editor/Panels/HierarchyLayer.hpp"
#include "Editor/SelectionContext.hpp"
#include "Frigga/Asset/AssetRegistry.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"
#include "Workflow.hpp"

#include <Freyr/Freyr.hpp>

class AnimationWorkflow: public Workflow
{
  public:
    AnimationWorkflow(skr::Arc<HierarchyLayer> hierarchy, skr::Arc<fg::AssetRegistry> assets,
                      skr::Arc<SelectionContext> selection, skr::Arc<fr::Registry> registry,
                      skr::Arc<fg::SceneSimulationState> simulation);

    ~AnimationWorkflow() = default;

    void buildDefaultDockLayout(ImGuiID dockspaceId) override;
};
