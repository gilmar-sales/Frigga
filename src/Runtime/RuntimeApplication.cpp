#include "RuntimeApplication.hpp"

#include <Frigga/Asset/AssetRegistry.hpp>
#include <Frigga/ECS/EcsLayout.hpp>
#include <Frigga/Input/InputMapIO.hpp>

#include <format>

RuntimeApplication::RuntimeApplication(const skr::Arc<skr::ServiceProvider> &serviceProvider)
    : fg::AbstractApplication(serviceProvider),
      mRegistry(serviceProvider->GetService<fr::Registry>()),
      mSystemManager(serviceProvider->GetService<fr::SystemManager>()),
      mScene(serviceProvider->GetService<fg::Scene>()),
      mInput(serviceProvider->GetService<fg::Input>()),
      mSimulation(serviceProvider->GetService<fg::SceneSimulationState>())
{
    const auto logger = serviceProvider->GetService<skr::Logger<RuntimeApplication>>();
    const auto project = serviceProvider->GetService<RuntimeProject>();
    const auto assets  = serviceProvider->GetService<fg::AssetRegistry>();
    const auto modules = serviceProvider->GetService<fg::GameplayModuleHost>();

    if(!project || !assets || !modules || !mScene || !mSimulation)
    {
        mWindow->Close();
        return;
    }

    assets->SetResourcesRoot(project->root / "Resources");
    if(mInput)
    {
        fg::InputMap map;
        std::string error;
        const auto inputPath = project->root / "input.json";
        if(std::filesystem::exists(inputPath) &&
           fg::LoadInputMapFile(inputPath, map, &error))
        {
            mInput->LoadBindings(map);
        }
        else if(!error.empty())
        {
            logger->LogWarning("Input bindings were not loaded: {}", error);
        }
        mInput->SetGameplayViewportHovered(true);
    }

    std::vector<fg::ModuleLoadRequest> requests;
    for(const auto &entry : project->modules)
    {
        if(!entry.enabled)
        {
            continue;
        }
        const auto library = project->root / entry.library;
        if(!std::filesystem::exists(library))
        {
            logger->LogError("Published module is missing: {}", library.string());
            mWindow->Close();
            return;
        }
        requests.push_back(fg::ModuleLoadRequest {
            .id          = entry.id,
            .name        = entry.name,
            .libraryPath = library,
        });
    }
    if(!modules->LoadAll(requests, /*stageForReload=*/false))
    {
        logger->LogError("Unable to load gameplay modules: {}", modules->GetLastError());
        mWindow->Close();
        return;
    }

    const auto ecsLayoutPath = project->root / fg::kEcsLayoutFileName;
    if(std::filesystem::exists(ecsLayoutPath))
    {
        fg::EcsLayout layout;
        std::string layoutError;
        if(!fg::LoadEcsLayoutFile(ecsLayoutPath, layout, &layoutError))
        {
            logger->LogError("Unable to load ECS layout: {}", layoutError);
            mWindow->Close();
            return;
        }
        const auto applied = fg::ApplyEcsLayout(*mRegistry, layout);
        if(!applied.ok)
        {
            logger->LogError("Unable to apply ECS layout: {}", applied.error);
            mWindow->Close();
            return;
        }
    }

    if(!mScene->LoadScene(project->ScenePath()))
    {
        logger->LogError("Unable to load startup scene: {}", project->ScenePath().string());
        mWindow->Close();
        return;
    }
    mScene->PreferGameplayCamera();

    for(const auto pipeline : {"Simulation", "Main", "Render"})
    {
        if(const auto pipelineId = mSystemManager->FindPipelineId(pipeline))
        {
            mSystemManager->SetPipelineEnabled(*pipelineId, true);
        }
    }
    mSimulation->Play();
}

void RuntimeApplication::RenderScene()
{
    if(mSimulation)
    {
        mSimulation->FlushPending();
    }
    for(const auto pipeline : {"Simulation", "Main", "Render"})
    {
        if(const auto pipelineId = mSystemManager->FindPipelineId(pipeline))
        {
            mSystemManager->SetPipelineEnabled(*pipelineId, true);
        }
    }
    if(mInput)
    {
        mInput->BeginFrame(mWindow->GetDeltaTime());
    }
    mRegistry->ExecuteTasks();
    mRegistry->Update(mWindow->GetDeltaTime());
}
