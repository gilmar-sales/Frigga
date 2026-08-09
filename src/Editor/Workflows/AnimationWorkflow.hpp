#pragma once

#include "Editor/Panels/HierarchyLayer.hpp"
#include "Editor/SelectionContext.hpp"
#include "Frigga/Asset/AssetRegistry.hpp"
#include "Frigga/Scene/Scene.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"
#include "Workflow.hpp"

#include <Freya/Freya.hpp>
#include <Freyr/Freyr.hpp>

class AnimationWorkflow: public Workflow
{
  public:
    AnimationWorkflow(skr::Arc<HierarchyLayer> hierarchy, skr::Arc<fg::AssetRegistry> assets,
                      skr::Arc<SelectionContext> selection, skr::Arc<fr::Registry> registry,
                      skr::Arc<fg::SceneSimulationState> simulation,
                      skr::Arc<fra::Renderer> renderer, skr::Arc<fra::MeshPool> meshPool,
                      skr::Arc<fg::Scene> scene);

    ~AnimationWorkflow() = default;

    void buildDefaultDockLayout(ImGuiID dockspaceId) override;
};
