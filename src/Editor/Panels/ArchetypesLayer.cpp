#include "ArchetypesLayer.hpp"

#include "Frigga/ECS/Components/NameComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"

#include <imgui.h>

ArchetypesLayer::ArchetypesLayer(Ref<fr::Scene> scene): mFreyrScene(scene) {}

void ArchetypesLayer::onGui()
{
    ImGui::Begin("Archetypes");

    ImGui::End();
}
