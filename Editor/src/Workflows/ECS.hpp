#pragma once

#include "Workflow.hpp"

class ECSLayer: public Workflow
{
  public:
    ECSLayer(): Workflow("ECS Layer", {}) {}

    ~ECSLayer() {}

    void onGui() override;
};
