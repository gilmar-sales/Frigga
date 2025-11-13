#include "EditorApplication.hpp"

void EditorApplication::Update()
{
    AbstractApplication::Update();
    mScene->ExecuteTasks();
}