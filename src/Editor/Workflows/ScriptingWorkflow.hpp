#pragma once

#include "Workflow.hpp"

class ScriptingWorkflow: public Workflow
{
  public:
    ScriptingWorkflow();

    ~ScriptingWorkflow() = default;

    void buildDefaultDockLayout(ImGuiID dockspaceId) override;
};
