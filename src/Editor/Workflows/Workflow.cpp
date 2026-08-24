#include "Workflow.hpp"

#include <imgui_internal.h>

Workflow::Workflow(std::string name, std::vector<skr::Arc<fg::Layer>> layers): fg::Layer(name)
{
    for(auto layer: layers)
    {
        m_layerStack.pushLayer(layer);
    }
}

void Workflow::onGui()
{
    for(auto layer: m_layerStack)
    {
        layer->onGui();
    }
}

void Workflow::onUpdate()
{
    for(auto layer: m_layerStack)
    {
        layer->onUpdate();
    }
}

void Workflow::onSuspend()
{
    for(auto layer: m_layerStack)
    {
        layer->onSuspend();
    }
}

void Workflow::onProcessDeferredReleases()
{
    for(auto layer: m_layerStack)
    {
        layer->onProcessDeferredReleases();
    }
}

void Workflow::onEvent(fg::Event &event)
{
    for(auto layer: m_layerStack)
    {
        layer->onEvent(event);
    }
}

void Workflow::buildDefaultDockLayout(ImGuiID dockspaceId)
{
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);
    ImGui::DockBuilderFinish(dockspaceId);
}
