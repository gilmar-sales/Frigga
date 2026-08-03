#pragma once

#include "Editor/Panels/HierarchyLayer.hpp"
#include "Workflow.hpp"

class AudioWorkflow: public Workflow
{
  public:
    AudioWorkflow(skr::Arc<HierarchyLayer> hierarchy);

    ~AudioWorkflow() = default;

    void buildDefaultDockLayout(ImGuiID dockspaceId) override;
};
