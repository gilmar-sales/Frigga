#pragma once

#include "Editor/Panels/HierarchyLayer.hpp"
#include "Editor/SelectionContext.hpp"
#include "Frigga/Asset/AssetRegistry.hpp"
#include "Frigga/Audio/AudioController.hpp"
#include "Frigga/Audio/IAudioEngine.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"
#include "Workflow.hpp"

#include <Freya/Core/Window.hpp>
#include <Freyr/Freyr.hpp>

class AudioWorkflow: public Workflow
{
  public:
    AudioWorkflow(skr::Arc<HierarchyLayer> hierarchy, skr::Arc<fg::AssetRegistry> assets,
                  skr::Arc<SelectionContext> selection, skr::Arc<fr::Registry> registry,
                  skr::Arc<fg::SceneSimulationState> simulation,
                  skr::Arc<fg::IAudioEngine> audioEngine, skr::Arc<fg::AudioController> controller,
                  skr::Arc<fra::Window> window);

    ~AudioWorkflow() override = default;

    void buildDefaultDockLayout(ImGuiID dockspaceId) override;
};
