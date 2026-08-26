#include "ModulesLayer.hpp"

#include "Editor/BoostrapIconsFont.hpp"
#include "Editor/DockLayout.hpp"
#include "Editor/Paths/EditorPaths.hpp"
#include "Editor/UiScale.hpp"

#include <imgui.h>

ModulesLayer::ModulesLayer(skr::Arc<ProjectSession> session,
                           skr::Arc<fg::GameplayModuleHost> moduleHost,
                           skr::Arc<fg::SceneSimulationState> simulation)
    : fg::Layer("Modules"), mSession(std::move(session)), mModuleHost(std::move(moduleHost)),
      mSimulation(std::move(simulation))
{
}

void ModulesLayer::onUpdate()
{
}

void ModulesLayer::refreshLibrary()
{
    EditorPaths::EnsureDirectories();
    mUserLibrary = ModuleCatalog::ScanDirectory(EditorPaths::DefaultModulesDir(), false);
    mBundled     = ModuleCatalog::ScanBundled(ProjectSession::DiscoverFriggaSdk(),
                                              ProjectSession::DiscoverFriggaRoot(),
                                              ProjectSession::ExecutablePath().parent_path());
}

void ModulesLayer::drawProjectModules()
{
    const auto &desc = mSession->GetDescriptor();
    ImGui::TextUnformatted("Project modules");
    ImGui::Separator();

    const bool playing = mSimulation && mSimulation->IsPlaying();
    ImGui::BeginDisabled(playing || !mSession->HasProject() || mSession->IsBuilding());

    ImGui::SetNextItemWidth(EditorUiScale::S(160.0f));
    ImGui::InputText("##NewModuleName", mNewName, sizeof(mNewName));
    ImGui::SameLine();
    if(ImGui::Button("Create"))
    {
        if(mSession->CreateModule(mNewName))
        {
            mStatus = mSession->GetStatusMessage();
        }
        else
        {
            mStatus = mSession->GetLastError();
        }
    }
    ImGui::SameLine();
    if(ImGui::Button("Build all"))
    {
        mSession->BuildModule();
    }
    ImGui::SameLine();
    if(ImGui::Button("Reload all"))
    {
        mSession->ReloadModule();
        mStatus = mSession->GetStatusMessage();
    }

    ImGui::EndDisabled();
    ImGui::Dummy(ImVec2(0.0f, EditorUiScale::S(6.0f)));

    if(ImGui::BeginTable("##ProjectModules", 5,
                         ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                             ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed, EditorUiScale::S(36.0f));
        ImGui::TableSetupColumn("Id");
        ImGui::TableSetupColumn("Loaded", ImGuiTableColumnFlags_WidthFixed, EditorUiScale::S(70.0f));
        ImGui::TableSetupColumn("Build", ImGuiTableColumnFlags_WidthFixed, EditorUiScale::S(70.0f));
        ImGui::TableSetupColumn("Share", ImGuiTableColumnFlags_WidthFixed, EditorUiScale::S(70.0f));
        ImGui::TableHeadersRow();

        for(const auto &entry : desc.modules)
        {
            ImGui::PushID(entry.id.c_str());
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            bool enabled = entry.enabled;
            ImGui::BeginDisabled(playing || mSession->IsBuilding());
            if(ImGui::Checkbox("##en", &enabled) && enabled != entry.enabled)
            {
                mSession->SetModuleEnabled(entry.id, enabled);
            }
            ImGui::EndDisabled();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(entry.id.c_str());
            ImGui::TableNextColumn();
            const bool loaded = mModuleHost && mModuleHost->IsModuleLoaded(entry.id);
            ImGui::TextUnformatted(loaded ? "yes" : "no");
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(playing || mSession->IsBuilding() || !entry.enabled);
            if(ImGui::SmallButton("Build"))
            {
                mSession->BuildModule(entry.target);
            }
            ImGui::EndDisabled();
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(entry.IsGameplay() || playing);
            if(ImGui::SmallButton("Export"))
            {
                if(mSession->ExportModule(entry.id))
                {
                    mStatus = mSession->GetStatusMessage();
                    refreshLibrary();
                }
                else
                {
                    mStatus = mSession->GetLastError();
                }
            }
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

void ModulesLayer::drawLibrary()
{
    ImGui::Dummy(ImVec2(0.0f, EditorUiScale::S(10.0f)));
    ImGui::TextUnformatted("Install");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", EditorPaths::DefaultModulesDir().string().c_str());
    ImGui::SameLine();
    if(ImGui::SmallButton("Refresh"))
    {
        refreshLibrary();
    }
    ImGui::Separator();

    const bool playing = mSimulation && mSimulation->IsPlaying();
    auto drawList      = [&](const char *heading, const std::vector<DiscoveredModule> &list) {
        ImGui::TextDisabled("%s", heading);
        if(list.empty())
        {
            ImGui::TextDisabled("  (none)");
            return;
        }
        for(const auto &module : list)
        {
            ImGui::PushID(module.root.string().c_str());
            ImGui::TextUnformatted(module.name.empty() ? module.id.c_str() : module.name.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("%s", module.id.c_str());
            ImGui::SameLine();
            ImGui::BeginDisabled(playing || !mSession->HasProject() || mSession->IsBuilding());
            if(ImGui::SmallButton("Install"))
            {
                if(mSession->InstallModuleFrom(module.root))
                {
                    mStatus = mSession->GetStatusMessage();
                }
                else
                {
                    mStatus = mSession->GetLastError();
                }
            }
            ImGui::EndDisabled();
            ImGui::PopID();
        }
    };

    drawList("Bundled", mBundled);
    drawList("User library", mUserLibrary);
}

void ModulesLayer::onGui()
{
    const auto windowId = EditorDock::WindowId("Modules");
    if(!ImGui::Begin(windowId.c_str()))
    {
        ImGui::End();
        return;
    }

    if(mUserLibrary.empty() && mBundled.empty())
    {
        refreshLibrary();
    }

    if(!mSession->HasProject())
    {
        ImGui::TextDisabled("Open a project to manage modules.");
        ImGui::End();
        return;
    }

    drawProjectModules();
    drawLibrary();

    if(!mStatus.empty())
    {
        ImGui::Separator();
        ImGui::TextWrapped("%s", mStatus.c_str());
    }

    ImGui::End();
}
