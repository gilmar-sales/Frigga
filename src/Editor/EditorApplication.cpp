#include "EditorApplication.hpp"

void EditorApplication::RenderScene()
{
    // Freyr defers CreateEntity/AddComponents into archetype task queues.
    // Those only drain via ExecuteTasks (or EnqueueTask while the pool is
    // already running). Update() alone runs systems but does not StartTasks on
    // archetypes — so Name/Transform/Camera data would stay defaulted.
    mRegistry->ExecuteTasks();
    mRegistry->Update(mWindow->GetDeltaTime());
}

void EditorApplication::Update()
{
    AbstractApplication::Update();
    // Flush entities created from Hierarchy/menus during onGui this frame.
    mRegistry->ExecuteTasks();
}
