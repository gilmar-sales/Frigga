#pragma once

#include "Editor/Panels/HierarchyLayer.hpp"
#include "Workflow.hpp"

class AnimationWorkflow: public Workflow
{
  public:
    AnimationWorkflow(skr::Arc<HierarchyLayer> hierarchy);

    ~AnimationWorkflow() = default;

    void buildDefaultDockLayout(ImGuiID dockspaceId) override;
};
