#pragma once

#include <Frigga/Core/Layer.hpp>
#include <Frigga/Core/LayerStack.hpp>

#include <imgui.h>

class Workflow: public fg::Layer
{
  public:
    Workflow(std::string name): fg::Layer(name) {}
    Workflow(std::string name, std::vector<skr::Arc<fg::Layer>> layers = {});

    ~Workflow() = default;

    void onGui() override;
    void onUpdate() override;
    void onSuspend() override;
    void onProcessDeferredReleases() override;
    void onEvent(fg::Event &event) override;

    // Builds a default dock tree for this workflow into the given dockspace.
    // Called when the dockspace has no saved layout, or on Reset Layout.
    virtual void buildDefaultDockLayout(ImGuiID dockspaceId);

  private:
    fg::LayerStack m_layerStack;
};
