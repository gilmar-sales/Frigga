#include "EditorApplication.hpp"

#include "UiScale.hpp"

#include <Freyr/Core/SystemManager.hpp>

void EditorApplication::syncPlayPipelines()
{
    if(!mSystemManager || !mSimulation)
    {
        return;
    }

    const bool playing = mSimulation->IsPlaying();
    if(const auto simId = mSystemManager->FindPipelineId("Simulation"))
    {
        mSystemManager->SetPipelineEnabled(*simId, playing);
    }
    if(const auto mainId = mSystemManager->FindPipelineId("Main"))
    {
        mSystemManager->SetPipelineEnabled(*mainId, playing);
    }
}

void EditorApplication::RenderScene()
{
    // Freyr defers CreateEntity/AddComponents into archetype task queues.
    // Those only drain via ExecuteTasks (or EnqueueTask while the pool is
    // already running). Update() alone runs systems but does not StartTasks on
    // archetypes — so Name/Transform/Camera data would stay defaulted.
    if(mSimulation)
    {
        mSimulation->FlushPending();
    }
    syncPlayPipelines();
    if(mInput)
    {
        mInput->BeginFrame(mWindow->GetDeltaTime());
    }
    mRegistry->ExecuteTasks();
    mRegistry->Update(mWindow->GetDeltaTime());
}

void EditorApplication::Update()
{
    // Poll every frame: monitor moves often skip DISPLAY_SCALE_CHANGED until resize.
    EditorUiScale::Sync(mWindow->GetScale());

    if(mSimulation)
    {
        mSimulation->SetDeferModeChanges(true);
    }
    AbstractApplication::Update();
    if(mSimulation)
    {
        mSimulation->SetDeferModeChanges(false);
    }
    // Flush entities created from Hierarchy/menus during onGui this frame.
    mRegistry->ExecuteTasks();
}
