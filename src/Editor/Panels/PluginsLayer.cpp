#include "PluginsLayer.hpp"

#include "Editor/BoostrapIconsFont.hpp"
#include "Editor/DockLayout.hpp"
#include "Editor/Paths/EditorPaths.hpp"
#include "Editor/UiScale.hpp"

#include <imgui.h>

PluginsLayer::PluginsLayer(skr::Arc<ProjectSession> session,
                           skr::Arc<fg::GameplayPluginHost> pluginHost,
                           skr::Arc<fg::SceneSimulationState> simulation)
    : fg::Layer("Plugins"), mSession(std::move(session)), mPluginHost(std::move(pluginHost)),
      mSimulation(std::move(simulation))
{
}

void PluginsLayer::onUpdate()
{
}

void PluginsLayer::refreshLibrary()
{
    EditorPaths::EnsureDirectories();
    mUserLibrary = PluginCatalog::ScanDirectory(EditorPaths::DefaultPluginsDir(), false);
    mBundled     = PluginCatalog::ScanBundled(ProjectSession::DiscoverFriggaSdk(),
                                              ProjectSession::DiscoverFriggaRoot(),
                                              ProjectSession::ExecutablePath().parent_path());
}

void PluginsLayer::drawProjectPlugins()
{
    const auto &desc = mSession->GetDescriptor();
    ImGui::TextUnformatted("Project plugins");
    ImGui::Separator();

    const bool playing = mSimulation && mSimulation->IsPlaying();
    ImGui::BeginDisabled(playing || !mSession->HasProject() || mSession->IsBuilding());

    ImGui::SetNextItemWidth(EditorUiScale::S(160.0f));
    ImGui::InputText("##NewPluginName", mNewName, sizeof(mNewName));
    ImGui::SameLine();
    if(ImGui::Button("Create"))
    {
        if(mSession->CreatePlugin(mNewName))
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
        mSession->BuildPlugin();
    }
    ImGui::SameLine();
    if(ImGui::Button("Reload all"))
    {
        mSession->ReloadPlugin();
        mStatus = mSession->GetStatusMessage();
    }

    ImGui::EndDisabled();
    ImGui::Dummy(ImVec2(0.0f, EditorUiScale::S(6.0f)));

    if(ImGui::BeginTable("##ProjectPlugins", 5,
                         ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                             ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed, EditorUiScale::S(36.0f));
        ImGui::TableSetupColumn("Id");
        ImGui::TableSetupColumn("Loaded", ImGuiTableColumnFlags_WidthFixed, EditorUiScale::S(70.0f));
        ImGui::TableSetupColumn("Build", ImGuiTableColumnFlags_WidthFixed, EditorUiScale::S(70.0f));
        ImGui::TableSetupColumn("Share", ImGuiTableColumnFlags_WidthFixed, EditorUiScale::S(70.0f));
        ImGui::TableHeadersRow();

        for(const auto &entry : desc.plugins)
        {
            ImGui::PushID(entry.id.c_str());
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            bool enabled = entry.enabled;
            ImGui::BeginDisabled(playing || mSession->IsBuilding());
            if(ImGui::Checkbox("##en", &enabled) && enabled != entry.enabled)
            {
                mSession->SetPluginEnabled(entry.id, enabled);
            }
            ImGui::EndDisabled();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(entry.id.c_str());
            ImGui::TableNextColumn();
            const bool loaded = mPluginHost && mPluginHost->IsPluginLoaded(entry.id);
            ImGui::TextUnformatted(loaded ? "yes" : "no");
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(playing || mSession->IsBuilding() || !entry.enabled);
            if(ImGui::SmallButton("Build"))
            {
                mSession->BuildPlugin(entry.target);
            }
            ImGui::EndDisabled();
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(entry.IsGameplay() || playing);
            if(ImGui::SmallButton("Export"))
            {
                if(mSession->ExportPlugin(entry.id))
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

void PluginsLayer::drawLibrary()
{
    ImGui::Dummy(ImVec2(0.0f, EditorUiScale::S(10.0f)));
    ImGui::TextUnformatted("Install");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", EditorPaths::DefaultPluginsDir().string().c_str());
    ImGui::SameLine();
    if(ImGui::SmallButton("Refresh"))
    {
        refreshLibrary();
    }
    ImGui::Separator();

    const bool playing = mSimulation && mSimulation->IsPlaying();
    auto drawList      = [&](const char *heading, const std::vector<DiscoveredPlugin> &list) {
        ImGui::TextDisabled("%s", heading);
        if(list.empty())
        {
            ImGui::TextDisabled("  (none)");
            return;
        }
        for(const auto &plugin : list)
        {
            ImGui::PushID(plugin.root.string().c_str());
            ImGui::TextUnformatted(plugin.name.empty() ? plugin.id.c_str() : plugin.name.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("%s", plugin.id.c_str());
            ImGui::SameLine();
            ImGui::BeginDisabled(playing || !mSession->HasProject() || mSession->IsBuilding());
            if(ImGui::SmallButton("Install"))
            {
                if(mSession->InstallPluginFrom(plugin.root))
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

void PluginsLayer::onGui()
{
    const auto windowId = EditorDock::WindowId("Plugins");
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
        ImGui::TextDisabled("Open a project to manage plugins.");
        ImGui::End();
        return;
    }

    drawProjectPlugins();
    drawLibrary();

    if(!mStatus.empty())
    {
        ImGui::Separator();
        ImGui::TextWrapped("%s", mStatus.c_str());
    }

    ImGui::End();
}
