#include "AudioBanksLayer.hpp"

#include "Editor/DockLayout.hpp"
#include "Editor/UiScale.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace
{
    constexpr std::size_t kFieldCapacity = 256;

    bool InputTextStd(const char *label, std::string &value, ImGuiInputTextFlags flags = 0)
    {
        char buffer[kFieldCapacity];
        std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
        if(ImGui::InputText(label, buffer, sizeof(buffer), flags))
        {
            value = buffer;
            return true;
        }
        return false;
    }

    void ClipField(fg::AssetRegistry &assets, std::string &clip)
    {
        ImGui::SetNextItemWidth(-EditorUiScale::S(34.0f));
        (void)InputTextStd("##clip", clip);
        ImGui::SameLine();
        if(ImGui::Button("..."))
        {
            ImGui::OpenPopup("##clip_pick");
        }
        if(ImGui::BeginPopup("##clip_pick"))
        {
            const auto &clips = assets.GetAudioClips();
            if(clips.empty())
            {
                ImGui::TextDisabled("No clips loaded.");
            }
            for(const auto &candidate : clips)
            {
                const bool selected = candidate.relativePath == clip;
                if(ImGui::Selectable(candidate.relativePath.c_str(), selected))
                {
                    clip = candidate.relativePath;
                }
            }
            ImGui::EndPopup();
        }
    }

    void BusField(fg::IAudioEngine &engine, std::string &bus)
    {
        const auto buses = engine.GetMixerBuses();
        if(ImGui::BeginCombo("##bus", bus.empty() ? "(none)" : bus.c_str()))
        {
            for(const auto &info : buses)
            {
                const bool selected = info.path == bus;
                if(ImGui::Selectable(info.path.c_str(), selected))
                {
                    bus = info.path;
                }
            }
            ImGui::EndCombo();
        }
    }
} // namespace

AudioBanksLayer::AudioBanksLayer(skr::Arc<fg::AssetRegistry> assets,
                                 skr::Arc<fg::IAudioEngine> audioEngine,
                                 skr::Arc<fg::AudioController> controller,
                                 skr::Arc<fg::SceneSimulationState> simulation)
    : Layer("Audio Banks"), mAssets(std::move(assets)), mAudioEngine(std::move(audioEngine)),
      mController(std::move(controller)), mSimulation(std::move(simulation))
{
}

void AudioBanksLayer::onGui()
{
    const auto title = EditorDock::WindowId(getName().c_str());
    if(!ImGui::Begin(title.c_str()))
    {
        ImGui::End();
        return;
    }

    if(mListDirty)
    {
        refreshBankList();
    }

    const bool playing = mSimulation != nullptr && mSimulation->IsPlaying();

    drawToolbar(playing);
    ImGui::Separator();

    if(ImGui::BeginTable("##AudioBanksSplit", 2,
                         ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("Banks", ImGuiTableColumnFlags_WidthFixed,
                                EditorUiScale::S(220.0f));
        ImGui::TableSetupColumn("Events", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        drawBankList(playing);

        ImGui::TableSetColumnIndex(1);
        drawEventEditor(playing);

        ImGui::EndTable();
    }

    if(!mStatus.empty())
    {
        ImGui::Spacing();
        ImGui::TextDisabled("%s", mStatus.c_str());
    }

    ImGui::End();
}

void AudioBanksLayer::refreshBankList()
{
    mBankFiles = fg::AssetRegistry::DiscoverBankFiles();

    for(const auto &bank : mAssets->GetBanks())
    {
        const std::filesystem::path relative(bank.relativePath);
        if(std::ranges::find(mBankFiles, relative) == mBankFiles.end())
        {
            mBankFiles.push_back(relative);
        }
    }
    std::ranges::sort(mBankFiles);
    mListDirty = false;
}

void AudioBanksLayer::selectBank(const std::filesystem::path &relativePath)
{
    if(const auto bank = mAssets->LoadBank(relativePath))
    {
        mSelectedBank = bank->relativePath;
        mDefinition   = bank->definition;
        mLoaded       = true;
        mDirty        = false;
        mStatus       = "Loaded " + mSelectedBank;
    }
    else
    {
        mStatus = "Failed to load " + relativePath.string();
    }
}

void AudioBanksLayer::createBank()
{
    std::string name = mNewBankName;
    const auto first = name.find_first_not_of(" \t");
    const auto last  = name.find_last_not_of(" \t");
    if(first == std::string::npos)
    {
        mStatus = "Enter a bank name first.";
        return;
    }
    name = name.substr(first, last - first + 1);

    if(name.find_first_of("/\\") != std::string::npos || name.find("..") != std::string::npos)
    {
        mStatus = "Bank name cannot contain path separators.";
        return;
    }
    if(!fg::AssetRegistry::IsBankFilename(name))
    {
        name += ".audiobank.json";
    }

    const std::filesystem::path relative = std::filesystem::path("Audio") / "Banks" / name;
    std::string error;
    if(!mAssets->SaveBank(relative.generic_string(), fg::AudioBankDefinition {}, &error))
    {
        mStatus = "Create failed: " + error;
        return;
    }

    mNewBankName[0] = '\0';
    mListDirty      = true;
    mStatus         = "Created " + relative.generic_string();
    selectBank(relative);
}

void AudioBanksLayer::drawToolbar(bool playing)
{
    ImGui::BeginDisabled(playing || !mLoaded || !mDirty);
    if(ImGui::Button("Save"))
    {
        std::string error;
        if(mAssets->SaveBank(mSelectedBank, mDefinition, &error))
        {
            mDirty  = false;
            mStatus = "Saved " + mSelectedBank;
            // Reload the definition so any engine-side filtering is reflected.
            if(const auto bank = mAssets->LoadBank(mSelectedBank))
            {
                mDefinition = bank->definition;
            }
        }
        else
        {
            mStatus = "Save failed: " + error;
        }
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(playing || !mLoaded);
    if(ImGui::Button("Reload"))
    {
        if(const auto bank = mAssets->ReloadBank(mSelectedBank))
        {
            mDefinition = bank->definition;
            mDirty      = false;
            mStatus     = "Reloaded " + mSelectedBank;
        }
        else
        {
            mStatus = "Reload failed for " + mSelectedBank;
        }
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if(ImGui::Button("Refresh"))
    {
        mListDirty = true;
        mStatus    = "Bank list refreshed.";
    }

    ImGui::SameLine();
    if(ImGui::Button("Stop Preview"))
    {
        mController->StopPreview();
    }

    ImGui::Separator();

    ImGui::SetNextItemWidth(EditorUiScale::S(180.0f));
    ImGui::InputTextWithHint("##NewBank", "New bank name...", mNewBankName, sizeof(mNewBankName));
    ImGui::SameLine();
    ImGui::BeginDisabled(playing);
    if(ImGui::Button("Create Bank"))
    {
        createBank();
    }
    ImGui::EndDisabled();

    if(mLoaded)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("%s%s", mSelectedBank.c_str(), mDirty ? " *" : "");
    }
}

void AudioBanksLayer::drawBankList(bool /*playing*/)
{
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##BankFilter", "Filter...", mFilter, sizeof(mFilter));

    const std::string filter = mFilter;
    if(ImGui::BeginChild("##BankList", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders))
    {
        for(const auto &relative : mBankFiles)
        {
            const std::string path = relative.generic_string();
            if(!filter.empty() && path.find(filter) == std::string::npos)
            {
                continue;
            }

            const bool selected = path == mSelectedBank;
            std::string label    = relative.filename().string();
            if(selected && mDirty)
            {
                label += " *";
            }
            if(ImGui::Selectable(label.c_str(), selected))
            {
                selectBank(relative);
            }
            if(ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", path.c_str());
            }
        }

        if(mBankFiles.empty())
        {
            ImGui::TextDisabled("No banks found under Resources/Audio/Banks.");
        }
    }
    ImGui::EndChild();
}

void AudioBanksLayer::drawEventEditor(bool playing)
{
    if(!mLoaded)
    {
        ImGui::TextDisabled("Select a bank to edit its events.");
        return;
    }

    if(mDefinition.events.empty())
    {
        ImGui::TextDisabled("This bank has no events.");
    }

    if(ImGui::Button("Add Event"))
    {
        fg::AudioBankEventDef event {};
        event.path = "event:/SFX/NewEvent";
        mDefinition.events.push_back(std::move(event));
        mDirty = true;
    }
    if(mDirty)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("Unsaved changes");
    }

    if(mDefinition.events.empty())
    {
        return;
    }

    int removeIndex = -1;
    int moveUp      = -1;
    int moveDown    = -1;

    constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                           ImGuiTableFlags_Resizable |
                                           ImGuiTableFlags_SizingStretchProp;

    if(ImGui::BeginTable("##BankEvents", 7, tableFlags))
    {
        ImGui::TableSetupColumn("Event", ImGuiTableColumnFlags_WidthStretch, 1.4f);
        ImGui::TableSetupColumn("Clip", ImGuiTableColumnFlags_WidthStretch, 1.4f);
        ImGui::TableSetupColumn("Volume", ImGuiTableColumnFlags_WidthFixed,
                                EditorUiScale::S(72.0f));
        ImGui::TableSetupColumn("Pitch", ImGuiTableColumnFlags_WidthFixed,
                                EditorUiScale::S(72.0f));
        ImGui::TableSetupColumn("Loop", ImGuiTableColumnFlags_WidthFixed,
                                EditorUiScale::S(42.0f));
        ImGui::TableSetupColumn("Bus", ImGuiTableColumnFlags_WidthFixed,
                                EditorUiScale::S(140.0f));
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed,
                                EditorUiScale::S(96.0f));
        ImGui::TableHeadersRow();

        for(int i = 0; i < static_cast<int>(mDefinition.events.size()); ++i)
        {
            auto &event = mDefinition.events[static_cast<std::size_t>(i)];
            ImGui::PushID(i);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            if(InputTextStd("##path", event.path))
            {
                mDirty = true;
            }
            if(event.path.empty())
            {
                ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.0f), "empty path");
            }

            ImGui::TableNextColumn();
            ImGui::BeginDisabled(playing);
            ClipField(*mAssets, event.clip);
            ImGui::EndDisabled();
            if(!event.clip.empty())
            {
                const auto absolute = fg::AssetRegistry::ToAbsoluteResourcePath(event.clip);
                if(!std::filesystem::is_regular_file(absolute))
                {
                    ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.0f), "missing clip");
                }
            }

            ImGui::TableNextColumn();
            ImGui::BeginDisabled(playing);
            ImGui::SetNextItemWidth(-1.0f);
            if(ImGui::DragFloat("##volume", &event.volume, 0.01f, 0.0f, 2.0f, "%.2f"))
            {
                mDirty = true;
            }

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1.0f);
            if(ImGui::DragFloat("##pitch", &event.pitch, 0.01f, 0.1f, 4.0f, "%.2f"))
            {
                mDirty = true;
            }

            ImGui::TableNextColumn();
            if(ImGui::Checkbox("##loop", &event.loop))
            {
                mDirty = true;
            }

            ImGui::TableNextColumn();
            const std::string previousBus = event.bus;
            BusField(*mAudioEngine, event.bus);
            if(event.bus != previousBus)
            {
                mDirty = true;
            }
            ImGui::EndDisabled();

            ImGui::TableNextColumn();
            ImGui::BeginDisabled(event.path.empty());
            if(ImGui::SmallButton("Play"))
            {
                (void)mController->PreviewEvent(event.path, event.volume, event.loop);
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if(ImGui::SmallButton("^"))
            {
                moveUp = i;
            }
            ImGui::SameLine();
            if(ImGui::SmallButton("v"))
            {
                moveDown = i;
            }
            ImGui::SameLine();
            if(ImGui::SmallButton("X"))
            {
                removeIndex = i;
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    if(removeIndex >= 0)
    {
        mDefinition.events.erase(mDefinition.events.begin() + removeIndex);
        mDirty = true;
    }
    if(moveUp > 0)
    {
        std::swap(mDefinition.events[static_cast<std::size_t>(moveUp)],
                  mDefinition.events[static_cast<std::size_t>(moveUp - 1)]);
        mDirty = true;
    }
    if(moveDown >= 0 && moveDown + 1 < static_cast<int>(mDefinition.events.size()))
    {
        std::swap(mDefinition.events[static_cast<std::size_t>(moveDown)],
                  mDefinition.events[static_cast<std::size_t>(moveDown + 1)]);
        mDirty = true;
    }

    std::size_t duplicates = 0;
    for(std::size_t i = 0; i < mDefinition.events.size(); ++i)
    {
        for(std::size_t j = i + 1; j < mDefinition.events.size(); ++j)
        {
            if(!mDefinition.events[i].path.empty() &&
               mDefinition.events[i].path == mDefinition.events[j].path)
            {
                ++duplicates;
            }
        }
    }
    if(duplicates > 0)
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.30f, 1.0f),
                           "%zu duplicate event path(s); later entries win.", duplicates);
    }
}
