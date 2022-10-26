#include "Resources.hpp"

#include <imgui.h>

void ResourcesLayer::onGui()
{
    ImGui::Begin(getName().c_str());
    ImGui::End();
}