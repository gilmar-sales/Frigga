#pragma once

#include "Editor/MaterialSelectionContext.hpp"
#include "Editor/Preferences/EditorPreferences.hpp"
#include "Frigga/Asset/AssetRegistry.hpp"
#include "Frigga/Asset/PrimitiveMeshFactory.hpp"
#include "Frigga/Scene/Scene.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"
#include "Workflow.hpp"

#include <Freya/Freya.hpp>
#include <Freyr/Freyr.hpp>

class ShadingWorkflow: public Workflow
{
  public:
    ShadingWorkflow(skr::Arc<fg::AssetRegistry> assets,
                    skr::Arc<fg::PrimitiveMeshFactory> primitives,
                    skr::Arc<MaterialSelectionContext> materialSelection,
                    skr::Arc<fra::Renderer> renderer, skr::Arc<fra::MeshPool> meshPool,
                    skr::Arc<fr::Registry> registry, skr::Arc<fg::Scene> scene,
                    skr::Arc<EditorPreferences> preferences,
                    skr::Arc<fg::SceneSimulationState> simulation, skr::Arc<fra::Window> window);

    ~ShadingWorkflow() = default;

    void buildDefaultDockLayout(ImGuiID dockspaceId) override;
};
