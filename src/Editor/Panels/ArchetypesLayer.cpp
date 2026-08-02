#include "ArchetypesLayer.hpp"

ArchetypesLayer::ArchetypesLayer(skr::Arc<fr::Registry> registry): mRegistry(std::move(registry)) {}

void ArchetypesLayer::onGui()
{
    ImGui::Begin("Archetypes");

    ImGui::End();
}
