#include "EditorApplication.hpp"

void EditorApplication::Update()
{
    AbstractApplication::Update();
    mRegistry->ExecuteTasks();
}
