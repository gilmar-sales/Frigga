#include "ServerApplication.hpp"

#include "Editor/Project/ProjectDescriptor.hpp"
#include "Editor/Project/ProjectFile.hpp"

#include <Frigga/Input/Input.hpp>

#include <chrono>
#include <csignal>
#include <thread>

#ifdef _WIN32
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    include <windows.h>
#endif

std::atomic<bool> ServerApplication::sRunning {true};

namespace
{
    std::filesystem::path ResolvePluginLibrary(const std::filesystem::path &root,
                                               const ProjectPluginEntry &entry)
    {
        const auto library =
            entry.libraryRelative.empty()
                ? ProjectDescriptor::DefaultLibraryRelative(
                      entry.target.empty() ? entry.id : entry.target)
                : entry.libraryRelative;
        const auto configured = root / library;
        std::error_code ec;
        if(std::filesystem::is_regular_file(configured, ec))
        {
            return configured;
        }

        const auto target = entry.target.empty()
                                ? (entry.id.empty() ? std::string("gameplay") : entry.id)
                                : entry.target;
#ifdef _WIN32
        const std::string names[] = {target + ".dll", "lib" + target + ".dll"};
#elif defined(__APPLE__)
        const std::string names[] = {"lib" + target + ".dylib", target + ".dylib"};
#else
        const std::string names[] = {"lib" + target + ".so", target + ".so"};
#endif
        const std::filesystem::path dirs[] = {
            configured.parent_path(),
            root / "build",
            root / "build" / "Debug",
            root / "build" / "Release",
        };
        for(const auto &dir : dirs)
        {
            if(dir.empty())
            {
                continue;
            }
            for(const auto &name : names)
            {
                const auto candidate = dir / name;
                if(std::filesystem::is_regular_file(candidate, ec))
                {
                    return candidate;
                }
            }
        }
        return configured;
    }

#ifdef _WIN32
    BOOL WINAPI OnConsoleCtrl(DWORD)
    {
        ServerApplication::RequestStop();
        return TRUE;
    }
#else
    void OnSignal(int)
    {
        ServerApplication::RequestStop();
    }
#endif
} // namespace

ServerApplication::ServerApplication(const skr::Arc<skr::ServiceProvider> &rootServiceProvider)
    : IApplication(rootServiceProvider)
{
    const auto &services = GetRootServiceProvider();
    mOptions    = services->GetService<ServerOptions>();
    mScene      = services->GetService<fg::Scene>();
    mSimulation = services->GetService<fg::SceneSimulationState>();
    mNetwork    = services->GetService<fg::Network>();
    mPlugins    = services->GetService<fg::GameplayPluginHost>();
    mInput      = services->GetService<fg::Input>();
    mRegistry   = services->GetService<fr::Registry>();
    mSystems    = services->GetService<fr::SystemManager>();
    mLogger     = services->GetService<skr::Logger<ServerApplication>>();
}

void ServerApplication::RequestStop()
{
    sRunning.store(false);
}

void ServerApplication::Run()
{
#ifdef _WIN32
    SetConsoleCtrlHandler(OnConsoleCtrl, TRUE);
#else
    std::signal(SIGINT, OnSignal);
    std::signal(SIGTERM, OnSignal);
#endif

    if(!mOptions || mOptions->projectFile.empty())
    {
        if(mLogger)
        {
            mLogger->LogError("Missing --project path to frigga.project");
        }
        return;
    }

    if(!loadProject())
    {
        return;
    }

    configurePipelines();
    mSimulation->Play();
    mSimulation->FlushPending();
    mRegistry->ExecuteTasks();

    if(!mNetwork->Host(mOptions->port, true))
    {
        if(mLogger)
        {
            mLogger->LogError("Failed to host: {}", mNetwork->LastError());
        }
        return;
    }

    if(mLogger)
    {
        mLogger->LogInformation("FriggaServer listening on port {}", mNetwork->Port());
    }

    tickLoop();
    mNetwork->Disconnect();
    mSimulation->Stop();
    mSimulation->FlushPending();
}

bool ServerApplication::loadProject()
{
    auto projectFile = mOptions->projectFile;
    if(std::filesystem::is_directory(projectFile))
    {
        projectFile /= ProjectFile::FileName;
    }
    if(!std::filesystem::exists(projectFile))
    {
        if(mLogger)
        {
            mLogger->LogError("Project file not found: {}", projectFile.string());
        }
        return false;
    }

    const auto desc = ProjectFile::Load(projectFile);
    if(!desc)
    {
        if(mLogger)
        {
            mLogger->LogError("Invalid frigga.project: {}", projectFile.string());
        }
        return false;
    }

    const auto root = projectFile.parent_path();
    std::error_code ec;
    std::filesystem::current_path(root, ec);

    auto scenePath = root / (mOptions->sceneRelative.empty() ? desc->sceneRelativePath
                                                             : mOptions->sceneRelative);
    if(!mScene->LoadScene(scenePath) && mLogger)
    {
        mLogger->LogWarning("Could not load scene {}, using default entities", scenePath.string());
    }

    if(!loadPlugins(*desc, root) && mLogger)
    {
        mLogger->LogWarning("No gameplay plugins loaded ({})", mPlugins->GetLastError());
    }
    return true;
}

bool ServerApplication::loadPlugins(const ProjectDescriptor &desc, const std::filesystem::path &root)
{
    ProjectDescriptor copy = desc;
    copy.EnsureGameplayPlugin();
    std::vector<fg::PluginLoadRequest> requests;
    for(const auto &entry : copy.LoadOrder())
    {
        const auto lib = ResolvePluginLibrary(root, entry);
        if(!std::filesystem::exists(lib))
        {
            continue;
        }
        requests.push_back(fg::PluginLoadRequest {
            .id = entry.id, .name = entry.id, .libraryPath = lib});
    }
    if(requests.empty())
    {
        return false;
    }
    return mPlugins->LoadAll(requests);
}

void ServerApplication::configurePipelines()
{
    if(!mSystems)
    {
        return;
    }
    if(const auto sim = mSystems->FindPipelineId("Simulation"))
    {
        mSystems->SetPipelineEnabled(*sim, true);
    }
    if(const auto main = mSystems->FindPipelineId("Main"))
    {
        mSystems->SetPipelineEnabled(*main, false);
    }
    if(const auto render = mSystems->FindPipelineId("Render"))
    {
        mSystems->SetPipelineEnabled(*render, false);
    }
}

void ServerApplication::tickLoop()
{
    using clock = std::chrono::steady_clock;
    const auto frame = std::chrono::duration<float>(1.0f / 60.0f);
    auto next        = clock::now();

    while(sRunning.load())
    {
        const auto now = clock::now();
        next += std::chrono::duration_cast<clock::duration>(frame);
        const float dt = frame.count();

        mSimulation->FlushPending();
        if(mInput)
        {
            mInput->BeginFrame(dt);
        }
        mRegistry->ExecuteTasks();
        mRegistry->Update(dt);

        if(now < next)
        {
            std::this_thread::sleep_until(next);
        }
        else
        {
            next = now;
        }
    }
}
