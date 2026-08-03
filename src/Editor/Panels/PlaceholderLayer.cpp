#include "PlaceholderLayer.hpp"

#include "Editor/DockLayout.hpp"

#include <imgui.h>

PlaceholderLayer::PlaceholderLayer(std::string title, std::string description)
    : fg::Layer(title), mTitle(std::move(title)), mDescription(std::move(description))
{
}

void PlaceholderLayer::onGui()
{
    const auto windowTitle = EditorDock::WindowId(mTitle.c_str());
    if(ImGui::Begin(windowTitle.c_str()))
    {
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const ImVec2 textSize = ImGui::CalcTextSize(mDescription.c_str(), nullptr, false, avail.x * 0.8f);

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail.x - textSize.x) * 0.5f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (avail.y - textSize.y) * 0.5f);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + avail.x * 0.8f);
        ImGui::TextDisabled("%s", mDescription.c_str());
        ImGui::PopTextWrapPos();
    }
    ImGui::End();
}
