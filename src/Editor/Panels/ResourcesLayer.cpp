#include "ResourcesLayer.hpp"

#include "Editor/DockLayout.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <imgui.h>

namespace
{
    constexpr const char *kResourcesRoot = "Resources";

    bool ContainsIgnoreCase(std::string_view haystack, std::string_view needle)
    {
        if(needle.empty())
        {
            return true;
        }

        auto lower = [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); };
        auto it    = std::ranges::search(haystack, needle, {}, lower, lower);
        return !it.empty();
    }

    bool ShouldSkipFile(const std::filesystem::path &path)
    {
        const auto ext = path.extension().string();
        return ext == ".spv";
    }

    int FolderSortPriority(std::string_view name)
    {
        if(name == "Primitives")
        {
            return 0;
        }
        if(name == "Textures")
        {
            return 1;
        }
        if(name == "Environments")
        {
            return 2;
        }
        if(name == "Shaders")
        {
            return 100;
        }
        return 50;
    }
} // namespace

ResourcesLayer::ResourcesLayer(skr::Arc<fg::PrimitiveMeshFactory> primitives)
    : fg::Layer("Resources"), mPrimitives(std::move(primitives))
{
}

bool ResourcesLayer::passesFilter(std::string_view text) const
{
    return ContainsIgnoreCase(text, mFilter);
}

void ResourcesLayer::refresh()
{
    mRoot          = AssetFolder {.name = kResourcesRoot};
    mNeedsRefresh  = false;

    AssetFolder primitivesFolder {.name = "Primitives"};
    for(std::uint8_t i = 0; i < static_cast<std::uint8_t>(fg::PrimitiveType::Count); ++i)
    {
        const auto type = static_cast<fg::PrimitiveType>(i);
        // Ensure mesh exists so inspector IDs stay resolvable after selection.
        (void)mPrimitives->GetMesh(type);
        primitivesFolder.entries.push_back(AssetEntry {
            .kind      = EntryKind::Primitive,
            .label     = fg::PrimitiveMeshFactory::GetDisplayName(type),
            .primitive = type,
        });
    }
    mRoot.children.push_back(std::move(primitivesFolder));

    const std::filesystem::path rootPath {kResourcesRoot};
    if(std::filesystem::exists(rootPath) && std::filesystem::is_directory(rootPath))
    {
        scanDirectory(rootPath, {}, mRoot);
    }

    std::ranges::sort(mRoot.children, [](const AssetFolder &a, const AssetFolder &b) {
        const int pa = FolderSortPriority(a.name);
        const int pb = FolderSortPriority(b.name);
        if(pa != pb)
        {
            return pa < pb;
        }
        return a.name < b.name;
    });
}

void ResourcesLayer::scanDirectory(const std::filesystem::path &absoluteDir,
                                   const std::filesystem::path &relativeDir, AssetFolder &folder)
{
    std::error_code ec;
    for(const auto &entry : std::filesystem::directory_iterator(absoluteDir, ec))
    {
        if(ec)
        {
            break;
        }

        const auto name = entry.path().filename().string();
        if(name.starts_with('.'))
        {
            continue;
        }

        if(entry.is_directory(ec))
        {
            AssetFolder child {.name = name};
            const auto childRelative =
                relativeDir.empty() ? std::filesystem::path {name} : relativeDir / name;
            scanDirectory(entry.path(), childRelative, child);

            if(!child.entries.empty() || !child.children.empty())
            {
                folder.children.push_back(std::move(child));
            }
            continue;
        }

        if(!entry.is_regular_file(ec) || ShouldSkipFile(entry.path()))
        {
            continue;
        }

        const auto relativePath =
            relativeDir.empty() ? std::filesystem::path {name} : relativeDir / name;
        folder.entries.push_back(AssetEntry {
            .kind         = EntryKind::File,
            .label        = name,
            .relativePath = relativePath,
        });
    }

    std::ranges::sort(folder.entries, [](const AssetEntry &a, const AssetEntry &b) {
        return a.label < b.label;
    });
    std::ranges::sort(folder.children, [](const AssetFolder &a, const AssetFolder &b) {
        const int pa = FolderSortPriority(a.name);
        const int pb = FolderSortPriority(b.name);
        if(pa != pb)
        {
            return pa < pb;
        }
        return a.name < b.name;
    });
}

bool ResourcesLayer::isSelected(const AssetEntry &entry) const
{
    if(!mSelected)
    {
        return false;
    }

    if(entry.kind != mSelected->kind)
    {
        return false;
    }

    if(entry.kind == EntryKind::Primitive)
    {
        return entry.primitive == mSelected->primitive;
    }

    return entry.relativePath == mSelected->relativePath;
}

void ResourcesLayer::selectEntry(const AssetEntry &entry)
{
    mSelected = entry;
}

void ResourcesLayer::drawEntry(const AssetEntry &entry)
{
    if(!passesFilter(entry.label) &&
       !(entry.kind == EntryKind::File && passesFilter(entry.relativePath.string())))
    {
        return;
    }

    const ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
        ImGuiTreeNodeFlags_SpanAvailWidth |
        (isSelected(entry) ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None);

    ImGui::TreeNodeEx(entry.label.c_str(), flags);
    if(ImGui::IsItemClicked())
    {
        selectEntry(entry);
    }
}

void ResourcesLayer::drawFolder(const AssetFolder &folder)
{
    bool anyVisible = mFilter.empty();
    if(!anyVisible)
    {
        for(const auto &entry : folder.entries)
        {
            if(passesFilter(entry.label) ||
               (entry.kind == EntryKind::File && passesFilter(entry.relativePath.string())))
            {
                anyVisible = true;
                break;
            }
        }
        if(!anyVisible)
        {
            // Keep walking children; they may match even if this folder name does not.
            anyVisible = true;
        }
    }

    if(!anyVisible)
    {
        return;
    }

    const bool openByDefault = folder.name != "Shaders";
    ImGui::SetNextItemOpen(openByDefault, ImGuiCond_Once);

    if(!ImGui::TreeNodeEx(folder.name.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth))
    {
        return;
    }

    for(const auto &child : folder.children)
    {
        drawFolder(child);
    }
    for(const auto &entry : folder.entries)
    {
        drawEntry(entry);
    }

    ImGui::TreePop();
}

void ResourcesLayer::onGui()
{
    if(mNeedsRefresh)
    {
        refresh();
    }

    const auto title = EditorDock::WindowId(getName().c_str());
    if(!ImGui::Begin(title.c_str()))
    {
        ImGui::End();
        return;
    }

    if(ImGui::Button("Refresh"))
    {
        refresh();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);
    char filterBuf[128] {};
    std::snprintf(filterBuf, sizeof(filterBuf), "%s", mFilter.c_str());
    if(ImGui::InputTextWithHint("##resource_filter", "Filter...", filterBuf, sizeof(filterBuf)))
    {
        mFilter = filterBuf;
    }

    ImGui::Separator();

    if(ImGui::BeginChild("##resource_browser", ImVec2(0.0f, -60.0f),
                         ImGuiChildFlags_Borders))
    {
        for(const auto &child : mRoot.children)
        {
            drawFolder(child);
        }
        for(const auto &entry : mRoot.entries)
        {
            drawEntry(entry);
        }
    }
    ImGui::EndChild();

    ImGui::Separator();
    if(mSelected)
    {
        if(mSelected->kind == EntryKind::Primitive)
        {
            ImGui::TextUnformatted("Primitive mesh");
            ImGui::Text("Name: %s", mSelected->label.c_str());
            ImGui::Text("Mesh ID: %u", mPrimitives->GetMesh(mSelected->primitive));
        }
        else
        {
            ImGui::TextUnformatted("File asset");
            ImGui::Text("Name: %s", mSelected->label.c_str());
            ImGui::TextWrapped("Path: Resources/%s", mSelected->relativePath.string().c_str());
        }
    }
    else
    {
        ImGui::TextDisabled("Select an asset to inspect.");
    }

    ImGui::End();
}
