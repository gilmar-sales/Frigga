#include "InputMapLayer.hpp"

#include "Editor/BoostrapIconsFont.hpp"
#include "Editor/UiScale.hpp"

#include <Frigga/Input/InputMapIO.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <span>
#include <string_view>
#include <imgui.h>

bool InputMapLayer::IsOpen = false;

namespace
{
    template <typename T>
    bool Contains(const std::vector<T> &values, T value)
    {
        return std::find(values.begin(), values.end(), value) != values.end();
    }

    int IndexOfName(std::span<const std::string_view> names, std::string_view current)
    {
        for(int i = 0; i < static_cast<int>(names.size()); ++i)
        {
            if(names[static_cast<std::size_t>(i)] == current)
            {
                return i;
            }
        }
        return 0;
    }
} // namespace

InputMapLayer::InputMapLayer(skr::Arc<ProjectSession> session, skr::Arc<fg::Input> input,
                             skr::Arc<fg::SceneSimulationState> simulation)
    : fg::Layer("Input Map"), mSession(std::move(session)), mInput(std::move(input)),
      mSimulation(std::move(simulation))
{
}

void InputMapLayer::onUpdate()
{
}

void InputMapLayer::markDirty()
{
    mDirty = true;
    mStatus.clear();
    mError.clear();
}

void InputMapLayer::applyToHost()
{
    mInput->LoadBindings(mDraft);
}

void InputMapLayer::saveToDisk()
{
    if(!mSession->HasProject())
    {
        mError = "No project open";
        return;
    }

    const auto path = *mSession->GetProjectRoot() / "input.json";
    std::string error;
    if(!fg::SaveInputMapFile(path, mDraft, &error))
    {
        mError = error.empty() ? "Failed to save input.json" : error;
        return;
    }

    applyToHost();
    mDirty  = false;
    mStatus = "Saved input.json";
    mError.clear();
}

void InputMapLayer::reloadFromDisk()
{
    if(!mSession->HasProject())
    {
        mDraft = mInput->GetBindings();
        mDirty = false;
        mStatus = "Reloaded from host";
        return;
    }

    const auto path = *mSession->GetProjectRoot() / "input.json";
    fg::InputMap map;
    std::string error;
    if(!std::filesystem::exists(path))
    {
        map = fg::MakeDefaultInputMap();
    }
    else if(!fg::LoadInputMapFile(path, map, &error))
    {
        mError = error.empty() ? "Failed to load input.json" : error;
        return;
    }

    mDraft = std::move(map);
    applyToHost();
    mDirty  = false;
    mStatus = "Reloaded input.json";
    mError.clear();

    if(mKind == SelectionKind::Action && !mDraft.actions.contains(mSelected))
    {
        mKind = SelectionKind::None;
        mSelected.clear();
    }
    if(mKind == SelectionKind::Axis && !mDraft.axes.contains(mSelected))
    {
        mKind = SelectionKind::None;
        mSelected.clear();
    }
}

void InputMapLayer::resetDefaults()
{
    mDraft = fg::MakeDefaultInputMap();
    applyToHost();
    markDirty();
    mStatus = "Reset to defaults (unsaved)";
    mKind   = SelectionKind::None;
    mSelected.clear();
}

void InputMapLayer::syncFromHostIfNeeded()
{
    std::string rootKey;
    if(const auto root = mSession->GetProjectRoot())
    {
        rootKey = root->string();
    }

    const bool opened = IsOpen && !mWasOpen;
    if(opened || rootKey != mProjectRootKey)
    {
        mProjectRootKey = rootKey;
        mDraft          = mInput->GetBindings();
        mDirty          = false;
        mStatus.clear();
        mError.clear();
        if(opened)
        {
            mKind = SelectionKind::None;
            mSelected.clear();
        }
    }
    mWasOpen = IsOpen;
}

std::vector<std::string> InputMapLayer::sortedActionNames() const
{
    std::vector<std::string> names;
    names.reserve(mDraft.actions.size());
    for(const auto &[name, _] : mDraft.actions)
    {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> InputMapLayer::sortedAxisNames() const
{
    std::vector<std::string> names;
    names.reserve(mDraft.axes.size());
    for(const auto &[name, _] : mDraft.axes)
    {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

void InputMapLayer::drawEnumChipList(const char *label, std::vector<fra::KeyCode> &values)
{
    ImGui::TextUnformatted(label);
    ImGui::Indent();
    int removeIndex = -1;
    for(int i = 0; i < static_cast<int>(values.size()); ++i)
    {
        ImGui::PushID(i);
        const auto name = fg::KeyName(values[static_cast<std::size_t>(i)]);
        if(ImGui::SmallButton(name.empty() ? "?" : name.data()))
        {
            removeIndex = i;
        }
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Click to remove");
        }
        if(i + 1 < static_cast<int>(values.size()))
        {
            ImGui::SameLine();
        }
        ImGui::PopID();
    }
    if(values.empty())
    {
        ImGui::TextDisabled("(none)");
    }
    ImGui::Unindent();
    if(removeIndex >= 0)
    {
        values.erase(values.begin() + removeIndex);
        markDirty();
    }
}

void InputMapLayer::drawEnumChipList(const char *label, std::vector<fra::MouseButton> &values)
{
    ImGui::TextUnformatted(label);
    ImGui::Indent();
    int removeIndex = -1;
    for(int i = 0; i < static_cast<int>(values.size()); ++i)
    {
        ImGui::PushID(i + 1000);
        const auto name = fg::MouseButtonName(values[static_cast<std::size_t>(i)]);
        if(ImGui::SmallButton(name.empty() ? "?" : name.data()))
        {
            removeIndex = i;
        }
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Click to remove");
        }
        if(i + 1 < static_cast<int>(values.size()))
        {
            ImGui::SameLine();
        }
        ImGui::PopID();
    }
    if(values.empty())
    {
        ImGui::TextDisabled("(none)");
    }
    ImGui::Unindent();
    if(removeIndex >= 0)
    {
        values.erase(values.begin() + removeIndex);
        markDirty();
    }
}

void InputMapLayer::drawEnumChipList(const char *label, std::vector<fra::GamepadButton> &values)
{
    ImGui::TextUnformatted(label);
    ImGui::Indent();
    int removeIndex = -1;
    for(int i = 0; i < static_cast<int>(values.size()); ++i)
    {
        ImGui::PushID(i + 2000);
        const auto name = fg::GamepadButtonName(values[static_cast<std::size_t>(i)]);
        if(ImGui::SmallButton(name.empty() ? "?" : name.data()))
        {
            removeIndex = i;
        }
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Click to remove");
        }
        if(i + 1 < static_cast<int>(values.size()))
        {
            ImGui::SameLine();
        }
        ImGui::PopID();
    }
    if(values.empty())
    {
        ImGui::TextDisabled("(none)");
    }
    ImGui::Unindent();
    if(removeIndex >= 0)
    {
        values.erase(values.begin() + removeIndex);
        markDirty();
    }
}

void InputMapLayer::drawKeyAddRow(const char *id, std::vector<fra::KeyCode> &values)
{
    const auto names = fg::KnownKeyNames();
    ImGui::PushID(id);
    ImGui::SetNextItemWidth(EditorUiScale::S(140.0f));
    if(ImGui::BeginCombo("##key", names[static_cast<std::size_t>(mAddKeyIndex)].data()))
    {
        for(int i = 0; i < static_cast<int>(names.size()); ++i)
        {
            const bool selected = i == mAddKeyIndex;
            if(ImGui::Selectable(names[static_cast<std::size_t>(i)].data(), selected))
            {
                mAddKeyIndex = i;
            }
            if(selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if(ImGui::Button("Add key"))
    {
        if(const auto parsed = fg::ParseKeyName(names[static_cast<std::size_t>(mAddKeyIndex)]))
        {
            if(!Contains(values, *parsed))
            {
                values.push_back(*parsed);
                markDirty();
            }
        }
    }
    ImGui::PopID();
}

void InputMapLayer::drawMouseAddRow(const char *id, std::vector<fra::MouseButton> &values)
{
    const auto names = fg::KnownMouseButtonNames();
    ImGui::PushID(id);
    ImGui::SetNextItemWidth(EditorUiScale::S(140.0f));
    if(ImGui::BeginCombo("##mouse", names[static_cast<std::size_t>(mAddMouseIndex)].data()))
    {
        for(int i = 0; i < static_cast<int>(names.size()); ++i)
        {
            const bool selected = i == mAddMouseIndex;
            if(ImGui::Selectable(names[static_cast<std::size_t>(i)].data(), selected))
            {
                mAddMouseIndex = i;
            }
            if(selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if(ImGui::Button("Add mouse"))
    {
        if(const auto parsed =
               fg::ParseMouseButtonName(names[static_cast<std::size_t>(mAddMouseIndex)]))
        {
            if(!Contains(values, *parsed))
            {
                values.push_back(*parsed);
                markDirty();
            }
        }
    }
    ImGui::PopID();
}

void InputMapLayer::drawGamepadAddRow(const char *id, std::vector<fra::GamepadButton> &values)
{
    const auto names = fg::KnownGamepadButtonNames();
    ImGui::PushID(id);
    ImGui::SetNextItemWidth(EditorUiScale::S(160.0f));
    if(ImGui::BeginCombo("##pad", names[static_cast<std::size_t>(mAddGamepadIndex)].data()))
    {
        for(int i = 0; i < static_cast<int>(names.size()); ++i)
        {
            const bool selected = i == mAddGamepadIndex;
            if(ImGui::Selectable(names[static_cast<std::size_t>(i)].data(), selected))
            {
                mAddGamepadIndex = i;
            }
            if(selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if(ImGui::Button("Add gamepad"))
    {
        if(const auto parsed =
               fg::ParseGamepadButtonName(names[static_cast<std::size_t>(mAddGamepadIndex)]))
        {
            if(!Contains(values, *parsed))
            {
                values.push_back(*parsed);
                markDirty();
            }
        }
    }
    ImGui::PopID();
}

void InputMapLayer::drawToolbar()
{
    const bool canEdit = mSession->HasProject() && mSession->IsInEditor();
    ImGui::BeginDisabled(!canEdit);

    if(ImGui::Button("Save"))
    {
        saveToDisk();
    }
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        ImGui::SetTooltip("Write project input.json and apply to runtime");
    }
    ImGui::SameLine();
    if(ImGui::Button("Reload"))
    {
        reloadFromDisk();
    }
    ImGui::SameLine();
    if(ImGui::Button("Apply"))
    {
        applyToHost();
        mStatus = "Applied (not saved)";
    }
    ImGui::SameLine();
    if(ImGui::Button("Reset defaults"))
    {
        resetDefaults();
    }

    ImGui::EndDisabled();

    if(mDirty)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "Unsaved");
    }
}

void InputMapLayer::drawActionList()
{
    ImGui::BeginChild("##ActionList", ImVec2(0, EditorUiScale::S(180.0f)),
                      ImGuiChildFlags_Borders);

    for(const auto &name : sortedActionNames())
    {
        const bool selected = mKind == SelectionKind::Action && mSelected == name;
        if(ImGui::Selectable(name.c_str(), selected))
        {
            mKind     = SelectionKind::Action;
            mSelected = name;
            std::snprintf(mRenameBuf, sizeof(mRenameBuf), "%s", name.c_str());
        }
        if(mSession->HasProject() && mSimulation->IsPlaying())
        {
            ImGui::SameLine();
            const bool down = mInput->IsDown(name);
            ImGui::TextColored(down ? ImVec4(0.35f, 0.9f, 0.45f, 1.0f)
                                    : ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                               down ? "down" : "up");
        }
    }

    ImGui::EndChild();

    ImGui::SetNextItemWidth(EditorUiScale::S(160.0f));
    ImGui::InputTextWithHint("##NewAction", "New action", mNewActionName, sizeof(mNewActionName));
    ImGui::SameLine();
    if(ImGui::Button("Add action"))
    {
        const std::string name = mNewActionName;
        if(!name.empty() && !mDraft.actions.contains(name))
        {
            mDraft.actions.emplace(name, fg::InputActionBinding {});
            mNewActionName[0] = '\0';
            mKind             = SelectionKind::Action;
            mSelected         = name;
            std::snprintf(mRenameBuf, sizeof(mRenameBuf), "%s", name.c_str());
            markDirty();
        }
    }
}

void InputMapLayer::drawAxisList()
{
    ImGui::BeginChild("##AxisList", ImVec2(0, EditorUiScale::S(180.0f)), ImGuiChildFlags_Borders);

    for(const auto &name : sortedAxisNames())
    {
        const bool selected = mKind == SelectionKind::Axis && mSelected == name;
        if(ImGui::Selectable(name.c_str(), selected))
        {
            mKind     = SelectionKind::Axis;
            mSelected = name;
            std::snprintf(mRenameBuf, sizeof(mRenameBuf), "%s", name.c_str());
        }
        if(mSession->HasProject() && mSimulation->IsPlaying())
        {
            ImGui::SameLine();
            const float value = mInput->GetAxis(name);
            ImGui::TextDisabled("%.2f", value);
        }
    }

    ImGui::EndChild();

    ImGui::SetNextItemWidth(EditorUiScale::S(160.0f));
    ImGui::InputTextWithHint("##NewAxis", "New axis", mNewAxisName, sizeof(mNewAxisName));
    ImGui::SameLine();
    if(ImGui::Button("Add axis"))
    {
        const std::string name = mNewAxisName;
        if(!name.empty() && !mDraft.axes.contains(name))
        {
            mDraft.axes.emplace(name, fg::InputAxisBinding {});
            mNewAxisName[0] = '\0';
            mKind           = SelectionKind::Axis;
            mSelected       = name;
            std::snprintf(mRenameBuf, sizeof(mRenameBuf), "%s", name.c_str());
            markDirty();
        }
    }
}

void InputMapLayer::drawActionEditor()
{
    auto it = mDraft.actions.find(mSelected);
    if(it == mDraft.actions.end())
    {
        ImGui::TextDisabled("Select an action.");
        return;
    }

    ImGui::Text("Action");
    ImGui::SetNextItemWidth(EditorUiScale::S(200.0f));
    ImGui::InputText("##RenameAction", mRenameBuf, sizeof(mRenameBuf));
    ImGui::SameLine();
    if(ImGui::Button("Rename"))
    {
        const std::string newName = mRenameBuf;
        if(!newName.empty() && newName != mSelected && !mDraft.actions.contains(newName))
        {
            auto binding = std::move(it->second);
            mDraft.actions.erase(it);
            mDraft.actions.emplace(newName, std::move(binding));
            mSelected = newName;
            markDirty();
            it = mDraft.actions.find(mSelected);
        }
    }
    ImGui::SameLine();
    if(ImGui::Button("Delete"))
    {
        mDraft.actions.erase(it);
        mKind = SelectionKind::None;
        mSelected.clear();
        markDirty();
        return;
    }

    auto &binding = it->second;
    ImGui::Separator();
    drawEnumChipList("Keys", binding.keys);
    drawKeyAddRow("action_keys", binding.keys);
    ImGui::Spacing();
    drawEnumChipList("Mouse", binding.mouseButtons);
    drawMouseAddRow("action_mouse", binding.mouseButtons);
    ImGui::Spacing();
    drawEnumChipList("Gamepad", binding.gamepadButtons);
    drawGamepadAddRow("action_pad", binding.gamepadButtons);
}

void InputMapLayer::drawAxisEditor()
{
    auto it = mDraft.axes.find(mSelected);
    if(it == mDraft.axes.end())
    {
        ImGui::TextDisabled("Select an axis.");
        return;
    }

    ImGui::Text("Axis");
    ImGui::SetNextItemWidth(EditorUiScale::S(200.0f));
    ImGui::InputText("##RenameAxis", mRenameBuf, sizeof(mRenameBuf));
    ImGui::SameLine();
    if(ImGui::Button("Rename"))
    {
        const std::string newName = mRenameBuf;
        if(!newName.empty() && newName != mSelected && !mDraft.axes.contains(newName))
        {
            auto binding = std::move(it->second);
            mDraft.axes.erase(it);
            mDraft.axes.emplace(newName, std::move(binding));
            mSelected = newName;
            markDirty();
            it = mDraft.axes.find(mSelected);
        }
    }
    ImGui::SameLine();
    if(ImGui::Button("Delete"))
    {
        mDraft.axes.erase(it);
        mKind = SelectionKind::None;
        mSelected.clear();
        markDirty();
        return;
    }

    auto &binding = it->second;
    ImGui::Separator();

    // Negative keys use a dedicated add index
    ImGui::TextUnformatted("Negative keys");
    ImGui::Indent();
    {
        int removeIndex = -1;
        for(int i = 0; i < static_cast<int>(binding.negativeKeys.size()); ++i)
        {
            ImGui::PushID(i + 3000);
            const auto name = fg::KeyName(binding.negativeKeys[static_cast<std::size_t>(i)]);
            if(ImGui::SmallButton(name.empty() ? "?" : name.data()))
            {
                removeIndex = i;
            }
            if(i + 1 < static_cast<int>(binding.negativeKeys.size()))
            {
                ImGui::SameLine();
            }
            ImGui::PopID();
        }
        if(binding.negativeKeys.empty())
        {
            ImGui::TextDisabled("(none)");
        }
        if(removeIndex >= 0)
        {
            binding.negativeKeys.erase(binding.negativeKeys.begin() + removeIndex);
            markDirty();
        }
    }
    ImGui::Unindent();
    {
        const auto names = fg::KnownKeyNames();
        ImGui::SetNextItemWidth(EditorUiScale::S(140.0f));
        if(ImGui::BeginCombo("##negKey", names[static_cast<std::size_t>(mAddNegKeyIndex)].data()))
        {
            for(int i = 0; i < static_cast<int>(names.size()); ++i)
            {
                const bool selected = i == mAddNegKeyIndex;
                if(ImGui::Selectable(names[static_cast<std::size_t>(i)].data(), selected))
                {
                    mAddNegKeyIndex = i;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if(ImGui::Button("Add neg"))
        {
            if(const auto parsed =
                   fg::ParseKeyName(names[static_cast<std::size_t>(mAddNegKeyIndex)]))
            {
                if(!Contains(binding.negativeKeys, *parsed))
                {
                    binding.negativeKeys.push_back(*parsed);
                    markDirty();
                }
            }
        }
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Positive keys");
    ImGui::Indent();
    {
        int removeIndex = -1;
        for(int i = 0; i < static_cast<int>(binding.positiveKeys.size()); ++i)
        {
            ImGui::PushID(i + 4000);
            const auto name = fg::KeyName(binding.positiveKeys[static_cast<std::size_t>(i)]);
            if(ImGui::SmallButton(name.empty() ? "?" : name.data()))
            {
                removeIndex = i;
            }
            if(i + 1 < static_cast<int>(binding.positiveKeys.size()))
            {
                ImGui::SameLine();
            }
            ImGui::PopID();
        }
        if(binding.positiveKeys.empty())
        {
            ImGui::TextDisabled("(none)");
        }
        if(removeIndex >= 0)
        {
            binding.positiveKeys.erase(binding.positiveKeys.begin() + removeIndex);
            markDirty();
        }
    }
    ImGui::Unindent();
    {
        const auto names = fg::KnownKeyNames();
        ImGui::SetNextItemWidth(EditorUiScale::S(140.0f));
        if(ImGui::BeginCombo("##posKey", names[static_cast<std::size_t>(mAddPosKeyIndex)].data()))
        {
            for(int i = 0; i < static_cast<int>(names.size()); ++i)
            {
                const bool selected = i == mAddPosKeyIndex;
                if(ImGui::Selectable(names[static_cast<std::size_t>(i)].data(), selected))
                {
                    mAddPosKeyIndex = i;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if(ImGui::Button("Add pos"))
        {
            if(const auto parsed =
                   fg::ParseKeyName(names[static_cast<std::size_t>(mAddPosKeyIndex)]))
            {
                if(!Contains(binding.positiveKeys, *parsed))
                {
                    binding.positiveKeys.push_back(*parsed);
                    markDirty();
                }
            }
        }
    }

    ImGui::Spacing();
    const auto axisNames = fg::KnownGamepadAxisNames();
    int axisIndex        = 0;
    std::string currentAxis = "(none)";
    if(binding.gamepadAxis)
    {
        const auto label = fg::GamepadAxisName(*binding.gamepadAxis);
        if(!label.empty())
        {
            currentAxis = std::string(label);
            axisIndex   = IndexOfName(axisNames, label);
        }
    }

    ImGui::SetNextItemWidth(EditorUiScale::S(160.0f));
    if(ImGui::BeginCombo("Gamepad axis", currentAxis.c_str()))
    {
        if(ImGui::Selectable("(none)", !binding.gamepadAxis.has_value()))
        {
            binding.gamepadAxis.reset();
            markDirty();
        }
        for(int i = 0; i < static_cast<int>(axisNames.size()); ++i)
        {
            const bool selected = binding.gamepadAxis.has_value() && i == axisIndex;
            if(ImGui::Selectable(axisNames[static_cast<std::size_t>(i)].data(), selected))
            {
                if(const auto parsed =
                       fg::ParseGamepadAxisName(axisNames[static_cast<std::size_t>(i)]))
                {
                    binding.gamepadAxis = *parsed;
                    markDirty();
                }
            }
        }
        ImGui::EndCombo();
    }

    if(ImGui::DragFloat("Deadzone", &binding.deadzone, 0.01f, 0.0f, 1.0f, "%.2f"))
    {
        markDirty();
    }
    if(ImGui::DragFloat("Scale", &binding.scale, 0.01f, -10.0f, 10.0f, "%.2f"))
    {
        markDirty();
    }
    if(ImGui::Checkbox("Invert gamepad", &binding.invertGamepad))
    {
        markDirty();
    }

    ImGui::Spacing();
    const auto mouseAxisNames = fg::KnownMouseAxisNames();
    int mouseAxisIndex        = 0;
    std::string currentMouse  = "(none)";
    if(binding.mouseAxis)
    {
        const auto label = fg::MouseAxisName(*binding.mouseAxis);
        if(!label.empty())
        {
            currentMouse   = std::string(label);
            mouseAxisIndex = IndexOfName(mouseAxisNames, label);
        }
    }
    ImGui::SetNextItemWidth(EditorUiScale::S(160.0f));
    if(ImGui::BeginCombo("Mouse axis", currentMouse.c_str()))
    {
        if(ImGui::Selectable("(none)", !binding.mouseAxis.has_value()))
        {
            binding.mouseAxis.reset();
            markDirty();
        }
        for(int i = 0; i < static_cast<int>(mouseAxisNames.size()); ++i)
        {
            const bool selected = binding.mouseAxis.has_value() && i == mouseAxisIndex;
            if(ImGui::Selectable(mouseAxisNames[static_cast<std::size_t>(i)].data(), selected))
            {
                if(const auto parsed =
                       fg::ParseMouseAxisName(mouseAxisNames[static_cast<std::size_t>(i)]))
                {
                    binding.mouseAxis = *parsed;
                    markDirty();
                }
            }
        }
        ImGui::EndCombo();
    }
    if(ImGui::DragFloat("Mouse scale", &binding.mouseScale, 0.01f, -10.0f, 10.0f, "%.3f"))
    {
        markDirty();
    }
    if(ImGui::Checkbox("Invert mouse", &binding.invertMouse))
    {
        markDirty();
    }
}

void InputMapLayer::drawLivePreview()
{
    if(!mSimulation->IsPlaying())
    {
        ImGui::TextDisabled("Enter Play to preview live action/axis state.");
        return;
    }

    ImGui::TextUnformatted("Live (host runtime)");
    if(mKind == SelectionKind::Action && !mSelected.empty())
    {
        ImGui::BulletText("%s  down=%s  pressed=%s  released=%s", mSelected.c_str(),
                          mInput->IsDown(mSelected) ? "yes" : "no",
                          mInput->WasPressed(mSelected) ? "yes" : "no",
                          mInput->WasReleased(mSelected) ? "yes" : "no");
    }
    else if(mKind == SelectionKind::Axis && !mSelected.empty())
    {
        ImGui::BulletText("%s  %.3f", mSelected.c_str(), mInput->GetAxis(mSelected));
    }
    else
    {
        ImGui::TextDisabled("Select an action or axis.");
    }
}

void InputMapLayer::onGui()
{
    if(!IsOpen)
    {
        mWasOpen = false;
        return;
    }

    if(!mSession->IsInEditor())
    {
        IsOpen   = false;
        mWasOpen = false;
        return;
    }

    syncFromHostIfNeeded();

    ImGui::SetNextWindowSize(ImVec2(EditorUiScale::S(780.0f), EditorUiScale::S(560.0f)),
                             ImGuiCond_FirstUseEver);
    if(!ImGui::Begin(ICON_BTSP_CONTROLLER " Input Map", &IsOpen, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    if(!mSession->HasProject())
    {
        ImGui::TextDisabled("Open a project to edit input.json.");
        ImGui::End();
        return;
    }

    drawToolbar();
    ImGui::Separator();

    if(ImGui::BeginTable("##InputMapSplit", 2,
                         ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("List", ImGuiTableColumnFlags_WidthFixed,
                                EditorUiScale::S(260.0f));
        ImGui::TableSetupColumn("Editor", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if(ImGui::BeginTabBar("##InputMapLists"))
        {
            if(ImGui::BeginTabItem("Actions"))
            {
                drawActionList();
                ImGui::EndTabItem();
            }
            if(ImGui::BeginTabItem("Axes"))
            {
                drawAxisList();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::TableSetColumnIndex(1);
        if(mKind == SelectionKind::Action)
        {
            drawActionEditor();
        }
        else if(mKind == SelectionKind::Axis)
        {
            drawAxisEditor();
        }
        else
        {
            ImGui::TextDisabled("Select an action or axis to edit bindings.");
        }

        ImGui::Spacing();
        ImGui::Separator();
        drawLivePreview();

        ImGui::EndTable();
    }

    if(!mStatus.empty())
    {
        ImGui::Spacing();
        ImGui::TextDisabled("%s", mStatus.c_str());
    }
    if(!mError.empty())
    {
        ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s", mError.c_str());
    }

    ImGui::End();
}
