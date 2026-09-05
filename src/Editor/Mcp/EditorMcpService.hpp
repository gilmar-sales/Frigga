#pragma once

#include "McpTransport.hpp"

#include "../Project/ProjectSession.hpp"

#include <Frigga/Scene/Scene.hpp>
#include <Frigga/Scene/SceneSimulationState.hpp>
#include <Skirnir/Logging/Logger.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

class EditorMcpService
{
  public:
    EditorMcpService(skr::Arc<ProjectSession> session, skr::Arc<fg::Scene> scene,
                     skr::Arc<fg::SceneSimulationState> simulation,
                     skr::Arc<skr::Logger<EditorMcpService>> logger);
    ~EditorMcpService();

    EditorMcpService(const EditorMcpService &) = delete;
    EditorMcpService &operator=(const EditorMcpService &) = delete;

    bool Start(std::string &error);
    void Stop();
    void Poll();

    [[nodiscard]] const std::filesystem::path &EndpointFile() const
    {
        return mEndpointFile;
    }

  private:
    struct Request
    {
        std::string id;
        std::string method;
        std::string params;
        std::promise<std::string> response;
    };

    std::string Dispatch(const Request &request);
    std::string ProjectInspect() const;
    std::string SceneInspect() const;
    std::string HandleSceneOpen(std::string_view params);
    std::string HandleSceneCreate(std::string_view params);
    std::string HandleSceneSave() const;
    std::string HandleSceneReplaceSnapshot(std::string_view params);
    std::string HandleAssetsValidate() const;
    std::string HandleAssetsList() const;
    std::string HandleAssetsCook(std::string_view params) const;
    std::string HandleLogsRecent() const;
    std::string HandleRuntime(std::string_view method);
    std::string HandleEditorInvoke(std::string_view params);

    void NetworkLoop();
    void FailPending(std::string_view message);

    skr::Arc<ProjectSession> mSession;
    skr::Arc<fg::Scene> mScene;
    skr::Arc<fg::SceneSimulationState> mSimulation;
    skr::Arc<skr::Logger<EditorMcpService>> mLogger;
    std::atomic<bool> mRunning {false};
    std::thread mNetworkThread;
    std::mutex mQueueMutex;
    std::queue<std::shared_ptr<Request>> mRequests;
    std::filesystem::path mEndpointFile;
    std::string mToken;
    std::atomic<std::intptr_t> mServerSocket {-1};
    std::atomic<std::intptr_t> mClientSocket {-1};
};
