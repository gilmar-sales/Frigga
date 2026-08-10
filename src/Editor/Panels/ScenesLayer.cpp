#include "ScenesLayer.hpp"

#include "Editor/DockLayout.hpp"
#include "Editor/UiScale.hpp"

#include <cstdio>
#include <imgui.h>

ScenesLayer::ScenesLayer(skr::Arc<ProjectSession> session, skr::Arc<fg::Scene> scene,
                         skr::Arc<fg::SceneSimulationState> simulation)
    : fg::Layer("Scenes"), mSession(std::move(session)), mScene(std::move(scene)),
      mSimulation(std::move(simulation))
{
}

void ScenesLayer::onUpdate()
{
}

void ScenesLayer::refresh()
{
    mScenes = mSession->ListSceneFiles();
}

void ScenesLayer::drawToolbar()
{
    const bool playing = mSimulation->IsPlaying();
    ImGui::BeginDisabled(playing || !mSession->HasProject());

    ImGui::SetNextItemWidth(EditorUiScale::S(160.0f));
    ImGui::InputText("##NewSceneName", mNewName, sizeof(mNewName));
    ImGui::SameLine();
    ImGui::RadioButton("3D", &mTemplateIndex, 0);
    ImGui::SameLine();
    ImGui::RadioButton("2D", &mTemplateIndex, 1);
    ImGui::SameLine();
    if(ImGui::Button("Create"))
    {
        const auto tmpl =
            mTemplateIndex == 1 ? fg::SceneTemplate::D2 : fg::SceneTemplate::D3;
        if(mSession->CreateScene(mNewName, tmpl, /*setAsStartup=*/false))
        {
            mStatus = "Created " + std::string(mNewName);
            refresh();
        }
        else
        {
            mStatus = mSession->GetLastError();
        }
    }
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        ImGui::SetTooltip("Create scenes/<name>.json from a template and open it");
    }

    ImGui::EndDisabled();
}

void ScenesLayer::drawList()
{
    const auto startup = mSession->GetDescriptor().sceneRelativePath;
    const auto current = mScene->GetPath();
    const auto root    = mSession->GetProjectRoot();

    if(ImGui::BeginTable("##ScenesTable", 3,
                         ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg |
                             ImGuiTableFlags_SizingStretchProp |
                             ImGuiTableFlags_ScrollY))
    {
        ImGui::TableSetupColumn("Scene", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Flags", ImGuiTableColumnFlags_WidthFixed,
                                EditorUiScale::S(90.0f));
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed,
                                EditorUiScale::S(160.0f));
        ImGui::TableHeadersRow();

        for(const auto &path : mScenes)
        {
            ImGui::PushID(path.string().c_str());
            ImGui::TableNextRow();

            std::string relative = path.filename().string();
            if(root)
            {
                std::error_code ec;
                const auto rel = std::filesystem::relative(path, *root, ec);
                if(!ec)
                {
                    relative = rel.generic_string();
                }
            }

            const bool isCurrent = [&] {
                if(current.empty())
                {
                    return false;
                }
                std::error_code ecA;
                std::error_code ecB;
                const auto a = std::filesystem::weakly_canonical(current, ecA);
                const auto b = std::filesystem::weakly_canonical(path, ecB);
                if(ecA || ecB)
                {
                    return current == path;
                }
                return a == b;
            }();
            const bool isStartup = relative == startup;

            ImGui::TableSetColumnIndex(0);
            if(ImGui::Selectable(relative.c_str(), isCurrent,
                                 ImGuiSelectableFlags_SpanAllColumns |
                                     ImGuiSelectableFlags_AllowOverlap))
            {
                if(!mSimulation->IsPlaying())
                {
                    if(mSession->OpenSceneFile(path))
                    {
                        mStatus = "Opened " + relative;
                    }
                    else
                    {
                        mStatus = mSession->GetLastError();
                    }
                }
            }
            if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                if(!mSimulation->IsPlaying())
                {
                    (void)mSession->OpenSceneFile(path);
                }
            }

            ImGui::TableSetColumnIndex(1);
            if(isCurrent)
            {
                ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.55f, 1.0f), "open");
                ImGui::SameLine();
            }
            if(isStartup)
            {
                ImGui::TextDisabled("startup");
            }

            ImGui::TableSetColumnIndex(2);
            ImGui::BeginDisabled(mSimulation->IsPlaying());
            if(ImGui::SmallButton("Open"))
            {
                if(mSession->OpenSceneFile(path))
                {
                    mStatus = "Opened " + relative;
                }
                else
                {
                    mStatus = mSession->GetLastError();
                }
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(isStartup);
            if(ImGui::SmallButton("Startup"))
            {
                if(mSession->SetStartupScene(path))
                {
                    mStatus = "Startup → " + relative;
                }
                else
                {
                    mStatus = mSession->GetLastError();
                }
            }
            ImGui::EndDisabled();
            ImGui::EndDisabled();

            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

void ScenesLayer::onGui()
{
    if(!mSession->IsInEditor())
    {
        return;
    }

    const auto windowId = EditorDock::WindowId("Scenes");
    if(!ImGui::Begin(windowId.c_str()))
    {
        ImGui::End();
        return;
    }

    if(!mSession->HasProject())
    {
        ImGui::TextDisabled("Open a project to manage scenes.");
        ImGui::End();
        return;
    }

    refresh();

    drawToolbar();
    ImGui::Separator();
    drawList();

    if(!mStatus.empty())
    {
        ImGui::Spacing();
        ImGui::TextDisabled("%s", mStatus.c_str());
    }
    const auto err = mSession->GetLastError();
    if(!err.empty() && err != mStatus)
    {
        ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s", err.c_str());
    }

    ImGui::End();
}
