#include "ScriptingWorkflow.hpp"

#include "Editor/DockLayout.hpp"
#include "Editor/Panels/PlaceholderLayer.hpp"

#include <imgui_internal.h>

ScriptingWorkflow::ScriptingWorkflow()
    : Workflow("Scripting",
               {
                   skr::MakeArc<PlaceholderLayer>(
                       "Scripts", "Browse project scripts and assemblies."),
                   skr::MakeArc<PlaceholderLayer>(
                       "Code Editor", "Edit gameplay scripts and tooling code."),
                   skr::MakeArc<PlaceholderLayer>(
                       "Console", "Logs, warnings, errors, and command input."),
                   skr::MakeArc<PlaceholderLayer>(
                       "Debugger", "Breakpoints, call stack, and watch values."),
               })
{
}

void ScriptingWorkflow::buildDefaultDockLayout(ImGuiID dockspaceId)
{
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

    ImGuiID mainId   = dockspaceId;
    ImGuiID leftId   = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Left, 0.18f, nullptr, &mainId);
    ImGuiID rightId  = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Right, 0.22f, nullptr, &mainId);
    ImGuiID bottomId = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Down, 0.28f, nullptr, &mainId);

    const auto scripts  = EditorDock::WindowId("Scripts");
    const auto editor   = EditorDock::WindowId("Code Editor");
    const auto console  = EditorDock::WindowId("Console");
    const auto debugger = EditorDock::WindowId("Debugger");

    ImGui::DockBuilderDockWindow(scripts.c_str(), leftId);
    ImGui::DockBuilderDockWindow(editor.c_str(), mainId);
    ImGui::DockBuilderDockWindow(debugger.c_str(), rightId);
    ImGui::DockBuilderDockWindow(console.c_str(), bottomId);

    ImGui::DockBuilderFinish(dockspaceId);
}
