#include "ComponentClipboard.hpp"

#include <imgui.h>

#include <string>

bool ComponentClipboard::Copy(const fg::Scene &scene, fr::Entity entity, std::string_view kind)
{
    std::string json;
    if(!fg::SceneSerializer::CopyComponent(scene, entity, kind, json))
    {
        return false;
    }

    sJson  = std::move(json);
    sKind  = kind;
    ImGui::SetClipboardText((std::string {kMagicPrefix} + sJson).c_str());
    return true;
}

bool ComponentClipboard::Paste(fg::Scene &scene, fr::Entity entity)
{
    const auto payload = ReadClipboardPayload();
    if(!payload)
    {
        return false;
    }
    return fg::SceneSerializer::PasteComponent(scene, entity, *payload);
}

bool ComponentClipboard::HasData()
{
    return ReadClipboardPayload().has_value();
}

std::string_view ComponentClipboard::GetKind()
{
    return sKind;
}

std::optional<std::string_view> ComponentClipboard::ReadClipboardPayload()
{
    if(!sJson.empty())
    {
        return sJson;
    }

    const char *clipboard = ImGui::GetClipboardText();
    if(clipboard == nullptr)
    {
        return std::nullopt;
    }

    const std::string_view text {clipboard};
    if(!text.starts_with(kMagicPrefix))
    {
        return std::nullopt;
    }

    return text.substr(std::string_view {kMagicPrefix}.size());
}
