#pragma once

#include "Workflow.hpp"

class ShadingWorkflow: public Workflow
{
  public:
    ShadingWorkflow();

    ~ShadingWorkflow() = default;

    void buildDefaultDockLayout(ImGuiID dockspaceId) override;
};
