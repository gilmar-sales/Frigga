#pragma once

#include "Editor/Project/ProjectSession.hpp"

#include <Frigga/Frigga.hpp>
#include <Frigga/Scene/SceneSimulationState.hpp>

#include <string>
#include <string_view>
#include <vector>

class PipelinesLayer: public fg::Layer
{
  public:
    PipelinesLayer(skr::Arc<fr::Registry> registry, skr::Arc<ProjectSession> session,
                   skr::Arc<fg::SceneSimulationState> simulation);
    ~PipelinesLayer() override = default;

    void onGui() override;

  private:
    void persistLayout();
    void addPipeline();
    void deleteUserPipeline(int32_t pipelineId);
    void registerEngineSystem(std::size_t catalogIndex, int32_t pipelineId);
    void drawPipeline(int32_t pipelineId, std::string_view name, float storedRate, bool enabled,
                      const std::vector<fr::SystemId> &systems, std::size_t index, std::size_t count);
    void drawSystemRow(fr::SystemId systemId, std::string_view label, int32_t pipelineId,
                       std::size_t index, std::size_t count);

    skr::Arc<fr::Registry> mRegistry;
    skr::Arc<ProjectSession> mSession;
    skr::Arc<fg::SceneSimulationState> mSimulation;

    char mNewName[64] = {};
    float mNewHz      = 0.0f;
    std::string mStatus;
    std::string mError;
    int32_t mPendingDeleteId = -1;
};
