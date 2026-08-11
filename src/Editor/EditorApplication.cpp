#include "EditorApplication.hpp"

#include "UiScale.hpp"

#include <Freyr/Core/SystemManager.hpp>

void EditorApplication::syncSimulationPipeline()
{
    if(!mSystemManager || !mSimulation)
    {
        return;
    }

    const auto simId = mSystemManager->FindPipelineId("Simulation");
    if(!simId)
    {
        return;
    }

    // IsPlaying covers pause + step: PhysicsSystem itself decides IsRunning vs step.
    mSystemManager->SetPipelineEnabled(*simId, mSimulation->IsPlaying());
}

void EditorApplication::RenderScene()
{
    // Freyr defers CreateEntity/AddComponents into archetype task queues.
    // Those only drain via ExecuteTasks (or EnqueueTask while the pool is
    // already running). Update() alone runs systems but does not StartTasks on
    // archetypes — so Name/Transform/Camera data would stay defaulted.
    syncSimulationPipeline();
    if(mInput)
    {
        mInput->BeginFrame();
    }
    mRegistry->ExecuteTasks();
    mRegistry->Update(mWindow->GetDeltaTime());
}

void EditorApplication::Update()
{
    // Poll every frame: monitor moves often skip DISPLAY_SCALE_CHANGED until resize.
    EditorUiScale::Sync(mWindow->GetScale());

    AbstractApplication::Update();
    // Flush entities created from Hierarchy/menus during onGui this frame.
    mRegistry->ExecuteTasks();
}
