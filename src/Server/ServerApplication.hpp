#pragma once

#include "Editor/Project/ProjectDescriptor.hpp"

#include <Frigga/Input/Input.hpp>
#include <Frigga/Net/Network.hpp>
#include <Frigga/Plugin/GameplayPluginHost.hpp>
#include <Frigga/Scene/Scene.hpp>
#include <Frigga/Scene/SceneSimulationState.hpp>

#include <Freyr/Core/SystemManager.hpp>
#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>

struct ServerOptions
{
    std::filesystem::path projectFile;
    std::uint16_t         port = 7777;
    std::string           sceneRelative;
};

class ServerApplication final: public skr::IApplication
{
  public:
    explicit ServerApplication(const skr::Arc<skr::ServiceProvider> &rootServiceProvider);
    void Run() override;

    static void RequestStop();

  private:
    bool loadProject();
    bool loadPlugins(const ProjectDescriptor &desc, const std::filesystem::path &root);
    void configurePipelines();
    void tickLoop();

    skr::Arc<ServerOptions> mOptions;
    skr::Arc<fg::Scene> mScene;
    skr::Arc<fg::SceneSimulationState> mSimulation;
    skr::Arc<fg::Network> mNetwork;
    skr::Arc<fg::GameplayPluginHost> mPlugins;
    skr::Arc<fg::Input> mInput;
    skr::Arc<fr::Registry> mRegistry;
    skr::Arc<fr::SystemManager> mSystems;
    skr::Arc<skr::Logger<ServerApplication>> mLogger;

    static std::atomic<bool> sRunning;
};
