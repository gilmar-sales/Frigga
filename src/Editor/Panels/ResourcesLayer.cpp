#include "ResourcesLayer.hpp"

#include "Editor/BoostrapIconsFont.hpp"
#include "Editor/DockLayout.hpp"
#include "Editor/Panels/HierarchyLayer.hpp"
#include "Editor/UiScale.hpp"
#include "Frigga/ECS/Components/AnimatorComponent.hpp"
#include "Frigga/ECS/Components/HierarchyComponent.hpp"
#include "Frigga/ECS/Components/MaterialComponent.hpp"
#include "Frigga/ECS/Components/MeshComponent.hpp"
#include "Frigga/ECS/Components/NameComponent.hpp"
#include "Frigga/ECS/Components/PrefabComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"
#include "Frigga/ECS/TransformUtil.hpp"
#include "Frigga/Scene/Prefab.hpp"

#include <SDL3/SDL_dialog.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <format>
#include <imgui.h>
#include <system_error>

namespace
{
    constexpr const char *kResourcesRoot = "Resources";

    const SDL_DialogFileFilter kModelFilters[] = {
        {"3D Models", "gltf;glb;fbx;obj;dae;3ds"},
        {"All files", "*"},
    };

    const SDL_DialogFileFilter kTextureFilters[] = {
        {"Images", "png;jpg;jpeg;tga;bmp;hdr;webp"},
        {"All files", "*"},
    };

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
        return ext == ".spv" || ext == ".mtl";
    }

    int FolderSortPriority(std::string_view name)
    {
        if(name == "Primitives")
        {
            return 0;
        }
        if(name == "Prefabs")
        {
            return 1;
        }
        if(name == "Models")
        {
            return 2;
        }
        if(name == "Textures")
        {
            return 3;
        }
        if(name == "Fonts")
        {
            return 4;
        }
        if(name == "Materials")
        {
            return 5;
        }
        if(name == "Environments")
        {
            return 6;
        }
        if(name == "Shaders")
        {
            return 100;
        }
        return 50;
    }

    bool IsEngineOnlyFolder(std::string_view name)
    {
        return name == "plugins" || name == "Shaders" || name == "ProjectTemplate";
    }

    bool IsStandardProjectFolder(std::string_view name)
    {
        return name == "Models" || name == "Textures" || name == "Prefabs" || name == "Fonts";
    }

    const char *KindLabel(ResourcesLayer::EntryKind kind)
    {
        switch(kind)
        {
        case ResourcesLayer::EntryKind::Primitive:
            return "Primitive";
        case ResourcesLayer::EntryKind::Model:
            return "Model";
        case ResourcesLayer::EntryKind::Texture:
            return "Texture";
        case ResourcesLayer::EntryKind::Material:
            return "Material";
        case ResourcesLayer::EntryKind::Prefab:
            return "Prefab";
        case ResourcesLayer::EntryKind::File:
            return "File";
        }
        return "File";
    }

    const char *KindIcon(ResourcesLayer::EntryKind kind)
    {
        switch(kind)
        {
        case ResourcesLayer::EntryKind::Primitive:
            return ICON_BTSP_BOXSEAM;
        case ResourcesLayer::EntryKind::Model:
            return ICON_BTSP_BOX;
        case ResourcesLayer::EntryKind::Texture:
            return ICON_BTSP_IMAGE;
        case ResourcesLayer::EntryKind::Material:
            return ICON_BTSP_LAYERS;
        case ResourcesLayer::EntryKind::Prefab:
            return ICON_BTSP_COLLECTION;
        case ResourcesLayer::EntryKind::File:
            return ICON_BTSP_FILE;
        }
        return ICON_BTSP_FILE;
    }

    ImVec4 KindColor(ResourcesLayer::EntryKind kind)
    {
        switch(kind)
        {
        case ResourcesLayer::EntryKind::Primitive:
            return {0.55f, 0.78f, 1.00f, 1.0f};
        case ResourcesLayer::EntryKind::Model:
            return {0.45f, 0.85f, 0.75f, 1.0f};
        case ResourcesLayer::EntryKind::Texture:
            return {0.95f, 0.72f, 0.40f, 1.0f};
        case ResourcesLayer::EntryKind::Material:
            return {0.78f, 0.55f, 0.95f, 1.0f};
        case ResourcesLayer::EntryKind::Prefab:
            return {0.95f, 0.62f, 0.35f, 1.0f};
        case ResourcesLayer::EntryKind::File:
            return {0.75f, 0.75f, 0.78f, 1.0f};
        }
        return {0.80f, 0.80f, 0.80f, 1.0f};
    }

    void SortFolder(ResourcesLayer::AssetFolder &folder)
    {
        std::ranges::sort(folder.entries, [](const auto &a, const auto &b) {
            return a.label < b.label;
        });
        std::ranges::sort(folder.children, [](const auto &a, const auto &b) {
            const int pa = FolderSortPriority(a.name);
            const int pb = FolderSortPriority(b.name);
            if(pa != pb)
            {
                return pa < pb;
            }
            return a.name < b.name;
        });
    }

    bool ViewModeButton(const char *id, const char *icon, const char *tooltip, bool active)
    {
        if(active)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
        const bool clicked = ImGui::Button(std::format("{}##{}", icon, id).c_str());
        if(active)
        {
            ImGui::PopStyleColor();
        }
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", tooltip);
        }
        return clicked;
    }
} // namespace

ResourcesLayer::ResourcesLayer(skr::Arc<fg::PrimitiveMeshFactory> primitives,
                               skr::Arc<fg::AssetRegistry> assets, skr::Arc<fr::Registry> registry,
                               skr::Arc<fg::Scene> scene, skr::Arc<SelectionContext> selection,
                               skr::Arc<fg::SceneSimulationState> simulation,
                               skr::Arc<fra::Window> window)
    : fg::Layer("Resources"), mPrimitives(std::move(primitives)), mAssets(std::move(assets)),
      mRegistry(std::move(registry)), mScene(std::move(scene)), mSelection(std::move(selection)),
      mSimulation(std::move(simulation)), mWindow(std::move(window))
{
}

void ResourcesLayer::MarkDirty()
{
    mNeedsRefresh = true;
}

bool ResourcesLayer::passesFilter(std::string_view text) const
{
    return ContainsIgnoreCase(text, mFilter);
}

void ResourcesLayer::refresh()
{
    mRoot         = AssetFolder {.name = kResourcesRoot};
    mNeedsRefresh = false;
    mScannedRoot  = fg::AssetRegistry::ResourcesRoot();

    AssetFolder primitivesFolder {.name = "Primitives"};
    for(std::uint8_t i = 0; i < static_cast<std::uint8_t>(fg::PrimitiveType::Count); ++i)
    {
        const auto type = static_cast<fg::PrimitiveType>(i);
        (void)mPrimitives->GetMesh(type);
        primitivesFolder.entries.push_back(AssetEntry {
            .kind      = EntryKind::Primitive,
            .label     = fg::PrimitiveMeshFactory::GetDisplayName(type),
            .primitive = type,
            .meshId    = mPrimitives->GetMesh(type),
        });
    }
    mRoot.children.push_back(std::move(primitivesFolder));

    AssetFolder materialsFolder {.name = "Materials"};
    materialsFolder.entries.push_back(AssetEntry {
        .kind       = EntryKind::Material,
        .label      = "Default",
        .materialId = mPrimitives->GetDefaultMaterial(),
    });
    for(const auto &material : mAssets->GetMaterials())
    {
        materialsFolder.entries.push_back(AssetEntry {
            .kind       = EntryKind::Material,
            .label      = material.name,
            .materialId = material.materialId,
        });
    }
    mRoot.children.push_back(std::move(materialsFolder));

    const auto rootPath = fg::AssetRegistry::ResourcesRoot();
    if(std::filesystem::exists(rootPath) && std::filesystem::is_directory(rootPath))
    {
        scanDirectory(rootPath, {}, mRoot);
    }

    ensureStandardFolders();
    SortFolder(mRoot);
}

void ResourcesLayer::ensureStandardFolders()
{
    auto ensure = [this](std::string_view name) {
        const bool has =
            std::ranges::any_of(mRoot.children, [&](const AssetFolder &folder) {
                return folder.name == name;
            });
        if(!has)
        {
            mRoot.children.push_back(AssetFolder {.name = std::string(name)});
        }
    };
    ensure("Models");
    ensure("Textures");
    ensure("Prefabs");
    ensure("Fonts");
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
        if(relativeDir.empty() && IsEngineOnlyFolder(name))
        {
            continue;
        }

        if(entry.is_directory(ec))
        {
            AssetFolder child {.name = name};
            const auto childRelative =
                relativeDir.empty() ? std::filesystem::path {name} : relativeDir / name;
            scanDirectory(entry.path(), childRelative, child);

            if(!child.entries.empty() || !child.children.empty() ||
               IsStandardProjectFolder(name))
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
        const auto ext = entry.path().extension().string();

        AssetEntry asset {
            .kind         = EntryKind::File,
            .label        = name,
            .relativePath = relativePath,
        };

        if(fg::AssetRegistry::IsModelExtension(ext))
        {
            asset.kind = EntryKind::Model;
        }
        else if(fg::AssetRegistry::IsTextureExtension(ext))
        {
            asset.kind = EntryKind::Texture;
        }
        else if(fg::AssetRegistry::IsPrefabExtension(ext))
        {
            asset.kind  = EntryKind::Prefab;
            asset.label = relativePath.stem().string();
        }

        folder.entries.push_back(std::move(asset));
    }

    SortFolder(folder);
}

const ResourcesLayer::AssetFolder *ResourcesLayer::currentFolder() const
{
    const AssetFolder *folder = &mRoot;
    for(const auto &name : mFolderPath)
    {
        const auto it = std::ranges::find_if(folder->children, [&](const AssetFolder &child) {
            return child.name == name;
        });
        if(it == folder->children.end())
        {
            return nullptr;
        }
        folder = &*it;
    }
    return folder;
}

void ResourcesLayer::enterFolder(std::string_view name)
{
    mFolderPath.emplace_back(name);
    mSelectedFolder.reset();
}

void ResourcesLayer::goUp()
{
    if(!mFolderPath.empty())
    {
        mFolderPath.pop_back();
        mSelectedFolder.reset();
    }
}

std::filesystem::path ResourcesLayer::currentRelativePath() const
{
    std::filesystem::path path;
    for(const auto &name : mFolderPath)
    {
        path /= name;
    }
    return path;
}

std::filesystem::path ResourcesLayer::writablePrefabFolder(const std::filesystem::path &relative)
{
    if(relative.empty())
    {
        return "Prefabs";
    }

    const auto first = relative.begin()->string();
    if(first == "Primitives" || first == "Materials")
    {
        return "Prefabs";
    }
    return relative;
}

void ResourcesLayer::acceptHierarchyPrefabDrop(const std::filesystem::path &destRelative)
{
    if(!ImGui::BeginDragDropTarget())
    {
        return;
    }

    const auto flags = ImGuiDragDropFlags_AcceptBeforeDelivery;
    if(const ImGuiPayload *payload =
           ImGui::AcceptDragDropPayload(HierarchyLayer::kDragPayloadId, flags))
    {
        const auto dest = writablePrefabFolder(destRelative);
        ImGui::SetTooltip("Create prefab in %s/", dest.generic_string().c_str());
        if(payload->IsDelivery())
        {
            const auto entity = *static_cast<const fr::Entity *>(payload->Data);
            (void)CreatePrefabFromEntity(entity, dest);
        }
    }
    ImGui::EndDragDropTarget();
}

bool ResourcesLayer::CreatePrefabFromEntity(fr::Entity entity, std::filesystem::path destRelative)
{
    if(mSimulation->IsPlaying() || entity == fg::kInvalidEntity)
    {
        mStatus = mSimulation->IsPlaying() ? "Stop Play mode to create prefabs"
                                           : "Invalid entity for prefab";
        return false;
    }

    destRelative = writablePrefabFolder(destRelative);

    std::string name = "Prefab";
    mRegistry->TryGetComponents<fg::NameComponent>(
        entity, [&](fg::NameComponent &component) { name = component.name; });

    const auto directory = fg::AssetRegistry::ToAbsoluteResourcePath(destRelative);
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);

    const auto path = fg::Prefab::UniqueAssetPath(directory, name);
    if(!fg::Prefab::Save(*mScene, entity, path))
    {
        mStatus = std::format("Failed to create prefab '{}'", path.string());
        return false;
    }

    auto relative = fg::AssetRegistry::MakeRelativeToResources(path);
    if(relative.empty())
    {
        relative = path.lexically_normal().generic_string();
    }
    const auto relativeStr = relative.generic_string();

    if(mRegistry->HasComponent<fg::PrefabComponent>(entity))
    {
        mRegistry->TryGetComponents<fg::PrefabComponent>(
            entity, [&](fg::PrefabComponent &prefab) { prefab.source = relativeStr; });
    }
    else
    {
        mRegistry->AddComponents(entity, fg::PrefabComponent {.source = relativeStr});
    }

    mFolderPath.clear();
    for(const auto &part : destRelative)
    {
        mFolderPath.push_back(part.string());
    }
    mSelected = AssetEntry {
        .kind         = EntryKind::Prefab,
        .label        = path.stem().string(),
        .relativePath = relative,
    };
    mSelectedFolder.reset();
    mNeedsRefresh = true;
    mStatus       = std::format("Created prefab '{}'", relativeStr);
    return true;
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

    switch(entry.kind)
    {
    case EntryKind::Primitive:
        return entry.primitive == mSelected->primitive;
    case EntryKind::Material:
        return entry.materialId == mSelected->materialId;
    case EntryKind::Model:
    case EntryKind::Texture:
    case EntryKind::Prefab:
    case EntryKind::File:
        return entry.relativePath == mSelected->relativePath;
    }
    return false;
}

void ResourcesLayer::selectEntry(const AssetEntry &entry)
{
    mSelected       = entry;
    mSelectedFolder.reset();
}

void ResourcesLayer::selectFolder(const AssetFolder &folder)
{
    mSelectedFolder = folder.name;
    mSelected.reset();
}

void ResourcesLayer::beginDrag(const AssetEntry &entry) const
{
    if(entry.kind != EntryKind::Primitive && entry.kind != EntryKind::Model &&
       entry.kind != EntryKind::Prefab && entry.kind != EntryKind::Material)
    {
        return;
    }

    if(ImGui::BeginDragDropSource())
    {
        ResourceDragPayload payload {.kind      = entry.kind,
                                     .primitive = entry.primitive,
                                     .materialId = entry.materialId,
                                     .meshId    = entry.meshId};
        const auto path = entry.relativePath.generic_string();
        std::snprintf(payload.relativePath, sizeof(payload.relativePath), "%s", path.c_str());
        ImGui::SetDragDropPayload(kDragPayloadId, &payload, sizeof(payload));
        ImGui::Text("%s %s", KindIcon(entry.kind), entry.label.c_str());
        ImGui::EndDragDropSource();
    }
}

void ResourcesLayer::handleEntryActivation(const AssetEntry &entry)
{
    selectEntry(entry);
    if(entry.kind == EntryKind::Primitive || entry.kind == EntryKind::Model ||
       entry.kind == EntryKind::Prefab)
    {
        spawnSelected();
    }
    else if(entry.kind == EntryKind::Material)
    {
        assignMaterialToSelection(entry.materialId);
    }
}

bool ResourcesLayer::drawFolderRow(const AssetFolder &folder, ViewMode mode)
{
    const bool selected = mSelectedFolder && *mSelectedFolder == folder.name;
    const auto flags    = ImGuiSelectableFlags_AllowDoubleClick |
                       (mode == ViewMode::Details ? ImGuiSelectableFlags_SpanAllColumns
                                                  : ImGuiSelectableFlags_None);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.82f, 0.40f, 1.0f));
    const std::string label = std::format("{}  {}", ICON_BTSP_FOLDER, folder.name);
    const bool clicked      = ImGui::Selectable(label.c_str(), selected, flags);
    ImGui::PopStyleColor();
    if(clicked)
    {
        selectFolder(folder);
    }
    if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        enterFolder(folder.name);
        return true;
    }
    acceptHierarchyPrefabDrop(currentRelativePath() / folder.name);
    return false;
}

bool ResourcesLayer::drawEntryRow(const AssetEntry &entry, ViewMode mode)
{
    if(!passesFilter(entry.label) &&
       !((entry.kind == EntryKind::File || entry.kind == EntryKind::Model ||
          entry.kind == EntryKind::Texture || entry.kind == EntryKind::Prefab) &&
         passesFilter(entry.relativePath.string())))
    {
        return false;
    }

    const bool selected = isSelected(entry);
    const auto flags    = ImGuiSelectableFlags_AllowDoubleClick |
                       (mode == ViewMode::Details ? ImGuiSelectableFlags_SpanAllColumns
                                                  : ImGuiSelectableFlags_None);
    ImGui::PushID(entry.label.c_str());
    ImGui::PushStyleColor(ImGuiCol_Text, KindColor(entry.kind));
    const std::string label = std::format("{}  {}", KindIcon(entry.kind), entry.label);
    const bool clicked      = ImGui::Selectable(label.c_str(), selected, flags);
    ImGui::PopStyleColor();
    if(clicked)
    {
        selectEntry(entry);
    }
    if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        handleEntryActivation(entry);
    }
    beginDrag(entry);

    if(ImGui::BeginPopupContextItem())
    {
        selectEntry(entry);
        const bool canSpawn = entry.kind == EntryKind::Primitive || entry.kind == EntryKind::Model ||
                              entry.kind == EntryKind::Prefab;
        if(ImGui::MenuItem("Add to Scene", nullptr, false, canSpawn && !mSimulation->IsPlaying()))
        {
            spawnSelected();
        }
        if(entry.kind == EntryKind::Material)
        {
            if(ImGui::MenuItem("Assign to Selection", nullptr, false,
                               mSelection->Get() != SelectionContext::Invalid &&
                                   !mSimulation->IsPlaying()))
            {
                assignMaterialToSelection(entry.materialId);
            }
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();
    return false;
}

void ResourcesLayer::drawListView(const AssetFolder &folder)
{
    for(const auto &child : folder.children)
    {
        if(!mFilter.empty() && !passesFilter(child.name))
        {
            continue;
        }
        if(drawFolderRow(child, ViewMode::List))
        {
            return;
        }
    }
    for(const auto &entry : folder.entries)
    {
        drawEntryRow(entry, ViewMode::List);
    }
}

void ResourcesLayer::drawGridView(const AssetFolder &folder)
{
    const float cellW = EditorUiScale::S(96.0f);
    const float cellH = EditorUiScale::S(108.0f);
    const float avail = ImGui::GetContentRegionAvail().x;
    const int columns = std::max(1, static_cast<int>(avail / cellW));
    int column        = 0;

    auto drawCell = [&](const char *id, const char *icon, const ImVec4 &color,
                        const std::string &label, bool selected, auto onClick, auto onActivate,
                        const std::filesystem::path *dropDest = nullptr) {
        ImGui::PushID(id);
        ImGui::BeginGroup();
        const ImVec2 size {cellW - EditorUiScale::S(8.0f), cellH - EditorUiScale::S(8.0f)};
        if(ImGui::Selectable("##cell", selected, ImGuiSelectableFlags_AllowDoubleClick, size))
        {
            onClick();
        }
        if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            onActivate();
        }
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        if(dropDest)
        {
            acceptHierarchyPrefabDrop(*dropDest);
        }
        ImDrawList *draw = ImGui::GetWindowDrawList();
        const ImVec2 iconPos {min.x + (max.x - min.x) * 0.5f,
                              min.y + EditorUiScale::S(28.0f)};
        draw->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 1.8f, iconPos, ImGui::GetColorU32(color),
                      icon);
        const ImVec2 textSize = ImGui::CalcTextSize(label.c_str(), nullptr, false, size.x - 8.0f);
        const ImVec2 textPos {min.x + (size.x - std::min(textSize.x, size.x - 8.0f)) * 0.5f,
                              max.y - EditorUiScale::S(28.0f)};
        const ImVec2 clipMax {max.x - 4.0f, max.y - 4.0f};
        draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), textPos, ImGui::GetColorU32(ImGuiCol_Text),
                      label.c_str(), nullptr, size.x - 8.0f, nullptr);
        (void)clipMax;
        ImGui::EndGroup();
        ImGui::PopID();
    };

    for(const auto &child : folder.children)
    {
        if(!mFilter.empty() && !passesFilter(child.name))
        {
            continue;
        }
        if(column > 0)
        {
            ImGui::SameLine();
        }
        const bool selected = mSelectedFolder && *mSelectedFolder == child.name;
        const auto id       = std::format("folder_{}", child.name);
        const auto dropDest = currentRelativePath() / child.name;
        drawCell(
            id.c_str(), ICON_BTSP_FOLDER, ImVec4(0.95f, 0.82f, 0.40f, 1.0f), child.name, selected,
            [&] { selectFolder(child); }, [&] { enterFolder(child.name); }, &dropDest);
        column = (column + 1) % columns;
        if(column == 0)
        {
            // next row
        }
    }

    for(const auto &entry : folder.entries)
    {
        if(!passesFilter(entry.label) &&
           !((entry.kind == EntryKind::File || entry.kind == EntryKind::Model ||
              entry.kind == EntryKind::Texture || entry.kind == EntryKind::Prefab) &&
             passesFilter(entry.relativePath.string())))
        {
            continue;
        }
        if(column > 0)
        {
            ImGui::SameLine();
        }
        const auto id = std::format("entry_{}_{}", static_cast<int>(entry.kind), entry.label);
        ImGui::PushID(id.c_str());
        ImGui::BeginGroup();
        const ImVec2 size {cellW - EditorUiScale::S(8.0f), cellH - EditorUiScale::S(8.0f)};
        if(ImGui::Selectable("##cell", isSelected(entry), ImGuiSelectableFlags_AllowDoubleClick,
                             size))
        {
            selectEntry(entry);
        }
        if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            handleEntryActivation(entry);
        }
        beginDrag(entry);
        if(ImGui::BeginPopupContextItem())
        {
            selectEntry(entry);
            const bool canSpawn = entry.kind == EntryKind::Primitive ||
                                  entry.kind == EntryKind::Model || entry.kind == EntryKind::Prefab;
            if(ImGui::MenuItem("Add to Scene", nullptr, false,
                               canSpawn && !mSimulation->IsPlaying()))
            {
                spawnSelected();
            }
            ImGui::EndPopup();
        }
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        ImDrawList *draw = ImGui::GetWindowDrawList();
        const char *icon = KindIcon(entry.kind);
        const ImVec2 iconSize = ImGui::CalcTextSize(icon);
        const ImVec2 iconPos {min.x + (max.x - min.x - iconSize.x * 1.8f) * 0.5f,
                              min.y + EditorUiScale::S(22.0f)};
        draw->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 1.8f, iconPos,
                      ImGui::GetColorU32(KindColor(entry.kind)), icon);
        draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                      ImVec2(min.x + 6.0f, max.y - EditorUiScale::S(26.0f)),
                      ImGui::GetColorU32(ImGuiCol_Text), entry.label.c_str(), nullptr,
                      size.x - 12.0f);
        ImGui::EndGroup();
        ImGui::PopID();
        column = (column + 1) % columns;
    }

    (void)cellH;
}

void ResourcesLayer::drawDetailsView(const AssetFolder &folder)
{
    constexpr ImGuiTableFlags flags =
        ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingStretchProp;
    if(!ImGui::BeginTable("##resource_details", 3, flags))
    {
        return;
    }

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.50f);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.20f);
    ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch, 0.30f);
    ImGui::TableHeadersRow();

    for(const auto &child : folder.children)
    {
        if(!mFilter.empty() && !passesFilter(child.name))
        {
            continue;
        }
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        if(drawFolderRow(child, ViewMode::Details))
        {
            ImGui::EndTable();
            return;
        }
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted("Folder");
        ImGui::TableSetColumnIndex(2);
        ImGui::TextDisabled("—");
    }

    for(const auto &entry : folder.entries)
    {
        if(!passesFilter(entry.label) &&
           !((entry.kind == EntryKind::File || entry.kind == EntryKind::Model ||
              entry.kind == EntryKind::Texture || entry.kind == EntryKind::Prefab) &&
             passesFilter(entry.relativePath.string())))
        {
            continue;
        }
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        drawEntryRow(entry, ViewMode::Details);
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(KindLabel(entry.kind));
        ImGui::TableSetColumnIndex(2);
        if(entry.relativePath.empty())
        {
            ImGui::TextDisabled("—");
        }
        else
        {
            ImGui::TextUnformatted(entry.relativePath.generic_string().c_str());
        }
    }

    ImGui::EndTable();
}

void ResourcesLayer::collectMatches(const AssetFolder &folder,
                                    std::vector<const AssetEntry *> &out) const
{
    for(const auto &entry : folder.entries)
    {
        if(passesFilter(entry.label) ||
           ((entry.kind == EntryKind::File || entry.kind == EntryKind::Model ||
             entry.kind == EntryKind::Texture || entry.kind == EntryKind::Prefab) &&
            passesFilter(entry.relativePath.string())))
        {
            out.push_back(&entry);
        }
    }
    for(const auto &child : folder.children)
    {
        collectMatches(child, out);
    }
}

void ResourcesLayer::drawSearchResults()
{
    std::vector<const AssetEntry *> matches;
    collectMatches(mRoot, matches);
    if(matches.empty())
    {
        ImGui::TextDisabled("No assets match \"%s\".", mFilter.c_str());
        return;
    }

    if(mViewMode == ViewMode::Details)
    {
        AssetFolder fake {.name = "Search"};
        for(const auto *entry : matches)
        {
            fake.entries.push_back(*entry);
        }
        drawDetailsView(fake);
        return;
    }

    for(const auto *entry : matches)
    {
        drawEntryRow(*entry, ViewMode::List);
    }
}

void ResourcesLayer::drawBreadcrumbs()
{
    ImGui::BeginDisabled(mFolderPath.empty());
    if(ImGui::Button(ICON_BTSP_ARROWLEFT))
    {
        goUp();
    }
    ImGui::EndDisabled();
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Up one folder");
    }

    ImGui::SameLine();
    if(ImGui::SmallButton(kResourcesRoot))
    {
        mFolderPath.clear();
        mSelectedFolder.reset();
    }

    std::vector<std::string> path;
    for(const auto &name : mFolderPath)
    {
        path.push_back(name);
        ImGui::SameLine();
        ImGui::TextUnformatted("/");
        ImGui::SameLine();
        const auto snapshot = path;
        if(ImGui::SmallButton(name.c_str()))
        {
            mFolderPath        = snapshot;
            mSelectedFolder.reset();
        }
    }
}

void ResourcesLayer::drawBrowser()
{
    if(!mFilter.empty() && mViewMode != ViewMode::Grid)
    {
        drawSearchResults();
        return;
    }

    const auto *folder = currentFolder();
    if(folder == nullptr)
    {
        mFolderPath.clear();
        folder = &mRoot;
    }

    if(!mFilter.empty() && mViewMode == ViewMode::Grid)
    {
        drawSearchResults();
        return;
    }

    switch(mViewMode)
    {
    case ViewMode::List:
        drawListView(*folder);
        break;
    case ViewMode::Grid:
        drawGridView(*folder);
        break;
    case ViewMode::Details:
        drawDetailsView(*folder);
        break;
    }
}

void ResourcesLayer::onImportModelDialog(void *userdata, const char *const *filelist, int)
{
    auto *self = static_cast<ResourcesLayer *>(userdata);
    if(filelist == nullptr || filelist[0] == nullptr)
    {
        return;
    }

    std::lock_guard lock(self->mDialogMutex);
    self->mPendingImport = PendingImport::Model;
    self->mPendingPath   = filelist[0];
}

void ResourcesLayer::onImportTextureDialog(void *userdata, const char *const *filelist, int)
{
    auto *self = static_cast<ResourcesLayer *>(userdata);
    if(filelist == nullptr || filelist[0] == nullptr)
    {
        return;
    }

    std::lock_guard lock(self->mDialogMutex);
    self->mPendingImport = PendingImport::Texture;
    self->mPendingPath   = filelist[0];
}

void ResourcesLayer::requestImportModel()
{
    SDL_ShowOpenFileDialog(onImportModelDialog, this, mWindow->Get(), kModelFilters,
                           static_cast<int>(std::size(kModelFilters)), nullptr, false);
}

void ResourcesLayer::requestImportTexture()
{
    SDL_ShowOpenFileDialog(onImportTextureDialog, this, mWindow->Get(), kTextureFilters,
                           static_cast<int>(std::size(kTextureFilters)), nullptr, false);
}

void ResourcesLayer::processPendingImports()
{
    PendingImport action = PendingImport::None;
    std::filesystem::path path;
    {
        std::lock_guard lock(mDialogMutex);
        if(mPendingImport == PendingImport::None || !mPendingPath)
        {
            return;
        }
        action         = mPendingImport;
        path           = *mPendingPath;
        mPendingImport = PendingImport::None;
        mPendingPath.reset();
    }

    if(action == PendingImport::Model)
    {
        if(const auto model = mAssets->ImportModel(path))
        {
            mStatus   = std::format("Imported model '{}'", model->relativePath);
            mSelected = AssetEntry {
                .kind         = EntryKind::Model,
                .label        = model->label,
                .relativePath = model->relativePath,
                .meshId       = model->meshIds.empty() ? 0u : model->meshIds.front(),
            };
            mNeedsRefresh = true;
        }
        else
        {
            mStatus = std::format("Failed to import model '{}'", path.string());
        }
    }
    else if(action == PendingImport::Texture)
    {
        if(const auto texture = mAssets->ImportTexture(path))
        {
            mStatus   = std::format("Imported texture '{}'", texture->relativePath);
            mSelected = AssetEntry {
                .kind         = EntryKind::Texture,
                .label        = texture->label,
                .relativePath = texture->relativePath,
                .textureId    = texture->textureId,
            };
            mNeedsRefresh = true;
        }
        else
        {
            mStatus = std::format("Failed to import texture '{}'", path.string());
        }
    }
}

void ResourcesLayer::spawnPrimitive(fg::PrimitiveType type)
{
    if(mSimulation->IsPlaying())
    {
        return;
    }

    const auto *displayName = fg::PrimitiveMeshFactory::GetDisplayName(type);
    spawnMesh(mPrimitives->GetMesh(type), displayName);
}

void ResourcesLayer::spawnMesh(std::uint32_t meshId, const std::string &name)
{
    if(mSimulation->IsPlaying() || meshId == 0)
    {
        return;
    }

    const auto entity = mRegistry->CreateEntity(
        fg::NameComponent {.name = name}, fg::TransformComponent {},
        fg::MeshComponent {.meshId = meshId},
        fg::MaterialComponent {.materialId = mPrimitives->GetDefaultMaterial()});
    mSelection->Select(entity);
    mStatus = std::format("Spawned '{}'", name);
}

void ResourcesLayer::spawnModel(const std::filesystem::path &relativePath)
{
    if(mSimulation->IsPlaying())
    {
        return;
    }

    const auto model = mAssets->LoadModel(relativePath);
    if(!model || model->meshIds.empty())
    {
        mStatus = std::format("Failed to load model '{}'", relativePath.string());
        return;
    }

    fr::Entity first = SelectionContext::Invalid;
    for(std::size_t i = 0; i < model->meshIds.size(); ++i)
    {
        const auto name = model->meshIds.size() == 1
                              ? model->label
                              : std::format("{} ({})", model->label, i);

        fr::Entity entity {};
        if(model->skinned && !model->clips.empty())
        {
            entity = mRegistry->CreateEntity(
                fg::NameComponent {.name = name},
                fg::TransformComponent {.scale = model->label.find("Fox") != std::string::npos
                                                     ? glm::vec3(0.02f)
                                                     : glm::vec3(1.0f)},
                fg::MeshComponent {.meshId = model->meshIds[i]},
                fg::MaterialComponent {.materialId = mPrimitives->GetDefaultMaterial()},
                fg::AnimatorComponent {.modelSource = model->relativePath, .playing = true,
                                       .previewInEdit = true});
        }
        else
        {
            entity = mRegistry->CreateEntity(
                fg::NameComponent {.name = name}, fg::TransformComponent {},
                fg::MeshComponent {.meshId = model->meshIds[i]},
                fg::MaterialComponent {.materialId = mPrimitives->GetDefaultMaterial()});
        }

        if(first == SelectionContext::Invalid)
        {
            first = entity;
        }
    }
    if(first != SelectionContext::Invalid)
    {
        mSelection->Select(first);
    }
    mStatus = std::format("Spawned model '{}' ({} meshes{})", model->label, model->meshIds.size(),
                          model->skinned ? ", skinned" : "");
}

void ResourcesLayer::spawnPrefab(const std::filesystem::path &relativePath)
{
    if(!InstantiatePrefab(relativePath))
    {
        return;
    }
}

bool ResourcesLayer::InstantiatePrefab(const std::filesystem::path &relativePath, fr::Entity parent)
{
    if(mSimulation->IsPlaying())
    {
        return false;
    }

    const auto absolute = fg::AssetRegistry::ToAbsoluteResourcePath(relativePath);
    fr::Entity root     = fg::kInvalidEntity;
    if(!fg::Prefab::Load(*mScene, absolute, parent, root))
    {
        mStatus = std::format("Failed to instantiate prefab '{}'", relativePath.string());
        return false;
    }

    mSelection->Select(root);
    mStatus = std::format("Instantiated prefab '{}'", relativePath.string());
    return true;
}

bool ResourcesLayer::HandleDrop(const ResourceDragPayload &payload, fr::Entity parent)
{
    if(mSimulation->IsPlaying())
    {
        return false;
    }

    switch(payload.kind)
    {
    case EntryKind::Prefab:
        return InstantiatePrefab(payload.relativePath, parent);
    case EntryKind::Primitive:
        spawnPrimitive(payload.primitive);
        if(parent != fg::kInvalidEntity && mSelection->Get() != SelectionContext::Invalid)
        {
            fg::TransformUtil::SetParent(*mRegistry, mSelection->Get(), parent, true);
        }
        return true;
    case EntryKind::Model:
        spawnModel(payload.relativePath);
        if(parent != fg::kInvalidEntity && mSelection->Get() != SelectionContext::Invalid)
        {
            fg::TransformUtil::SetParent(*mRegistry, mSelection->Get(), parent, true);
        }
        return true;
    case EntryKind::Material:
        if(parent != fg::kInvalidEntity)
        {
            const auto previous = mSelection->Get();
            mSelection->Select(parent);
            assignMaterialToSelection(payload.materialId);
            mSelection->Select(previous);
            return true;
        }
        assignMaterialToSelection(payload.materialId);
        return true;
    default:
        return false;
    }
}

void ResourcesLayer::spawnSelected()
{
    if(!mSelected)
    {
        return;
    }

    switch(mSelected->kind)
    {
    case EntryKind::Primitive:
        spawnPrimitive(mSelected->primitive);
        break;
    case EntryKind::Model:
        spawnModel(mSelected->relativePath);
        break;
    case EntryKind::Prefab:
        spawnPrefab(mSelected->relativePath);
        break;
    default:
        break;
    }
}

void ResourcesLayer::createMaterialAsset()
{
    if(mSimulation->IsPlaying())
    {
        return;
    }

    const auto source = mPrimitives->GetMaterialCreateInfo(mPrimitives->GetDefaultMaterial());
    const auto name   = std::format("Material {}", mAssets->GetMaterials().size() + 1);
    const auto id     = mAssets->CreateMaterial(source, name);
    mSelected         = AssetEntry {
                .kind       = EntryKind::Material,
                .label      = name,
                .materialId = id,
    };
    mNeedsRefresh = true;
    mStatus       = std::format("Created material '{}'", name);
}

void ResourcesLayer::assignMaterialToSelection(std::uint32_t materialId)
{
    if(mSimulation->IsPlaying())
    {
        return;
    }

    const auto entity = mSelection->Get();
    if(entity == SelectionContext::Invalid)
    {
        mStatus = "No selection to assign material";
        return;
    }

    if(mRegistry->HasComponent<fg::MaterialComponent>(entity))
    {
        mRegistry->TryGetComponents<fg::MaterialComponent>(
            entity, [&](fg::MaterialComponent &material) { material.materialId = materialId; });
    }
    else
    {
        mRegistry->AddComponents(entity, fg::MaterialComponent {.materialId = materialId});
    }
    mStatus = "Assigned material to selection";
}

void ResourcesLayer::drawToolbar()
{
    const bool playing = mSimulation->IsPlaying();
    ImGui::BeginDisabled(playing);
    if(ImGui::Button(ICON_BTSP_BOX " Import Model"))
    {
        requestImportModel();
    }
    ImGui::SameLine();
    if(ImGui::Button(ICON_BTSP_IMAGE " Import Texture"))
    {
        requestImportTexture();
    }
    ImGui::SameLine();
    if(ImGui::Button(ICON_BTSP_LAYERS " New Material"))
    {
        createMaterialAsset();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if(ImGui::Button(ICON_BTSP_RELOAD " Refresh"))
    {
        mNeedsRefresh = true;
    }

    ImGui::SameLine();
    const float buttonsWidth = ImGui::CalcTextSize(ICON_BTSP_FILETEXT ICON_BTSP_COLLECTION
                                                   ICON_BTSP_WINDOWS)
                                   .x +
                               ImGui::GetStyle().ItemSpacing.x * 6.0f +
                               ImGui::GetStyle().FramePadding.x * 6.0f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                         std::max(0.0f, ImGui::GetContentRegionAvail().x - buttonsWidth));
    if(ViewModeButton("list", ICON_BTSP_FILETEXT, "List", mViewMode == ViewMode::List))
    {
        mViewMode = ViewMode::List;
    }
    ImGui::SameLine();
    if(ViewModeButton("grid", ICON_BTSP_COLLECTION, "Grid", mViewMode == ViewMode::Grid))
    {
        mViewMode = ViewMode::Grid;
    }
    ImGui::SameLine();
    if(ViewModeButton("details", ICON_BTSP_SLIDERS, "Details", mViewMode == ViewMode::Details))
    {
        mViewMode = ViewMode::Details;
    }
}

void ResourcesLayer::drawInspector()
{
    if(mSelectedFolder)
    {
        ImGui::TextUnformatted(ICON_BTSP_FOLDER " Folder");
        ImGui::Text("Name: %s", mSelectedFolder->c_str());
        if(ImGui::Button("Open"))
        {
            enterFolder(*mSelectedFolder);
        }
        return;
    }

    if(!mSelected)
    {
        ImGui::TextDisabled("Select an asset to inspect.");
        return;
    }

    ImGui::Text("%s %s", KindIcon(mSelected->kind), KindLabel(mSelected->kind));
    ImGui::Text("Name: %s", mSelected->label.c_str());

    switch(mSelected->kind)
    {
    case EntryKind::Primitive:
        ImGui::Text("Mesh ID: %u", mPrimitives->GetMesh(mSelected->primitive));
        ImGui::BeginDisabled(mSimulation->IsPlaying());
        if(ImGui::Button("Add to Scene"))
        {
            spawnPrimitive(mSelected->primitive);
        }
        ImGui::EndDisabled();
        break;
    case EntryKind::Model:
    {
        ImGui::TextWrapped("Path: Resources/%s", mSelected->relativePath.string().c_str());
        if(const auto model = mAssets->LoadModel(mSelected->relativePath))
        {
            ImGui::Text("Meshes: %zu", model->meshIds.size());
            for(std::size_t i = 0; i < model->meshIds.size(); ++i)
            {
                ImGui::BulletText("[%zu] Mesh ID %u", i, model->meshIds[i]);
            }
        }
        ImGui::BeginDisabled(mSimulation->IsPlaying());
        if(ImGui::Button("Add to Scene"))
        {
            spawnModel(mSelected->relativePath);
        }
        ImGui::EndDisabled();
        break;
    }
    case EntryKind::Texture:
    {
        ImGui::TextWrapped("Path: Resources/%s", mSelected->relativePath.string().c_str());
        if(const auto texture = mAssets->LoadTexture(mSelected->relativePath))
        {
            mSelected->textureId = texture->textureId;
            ImGui::Text("Texture ID: %u", texture->textureId);
        }
        break;
    }
    case EntryKind::Material:
        ImGui::Text("Material ID: %u", mSelected->materialId);
        ImGui::BeginDisabled(mSimulation->IsPlaying() ||
                             mSelection->Get() == SelectionContext::Invalid);
        if(ImGui::Button("Assign to Selection"))
        {
            assignMaterialToSelection(mSelected->materialId);
        }
        ImGui::EndDisabled();
        break;
    case EntryKind::Prefab:
        ImGui::TextWrapped("Path: Resources/%s", mSelected->relativePath.string().c_str());
        ImGui::BeginDisabled(mSimulation->IsPlaying());
        if(ImGui::Button("Add to Scene"))
        {
            spawnPrefab(mSelected->relativePath);
        }
        ImGui::EndDisabled();
        break;
    case EntryKind::File:
        ImGui::TextWrapped("Path: Resources/%s", mSelected->relativePath.string().c_str());
        break;
    }

    if(!mStatus.empty())
    {
        ImGui::Separator();
        ImGui::TextWrapped("%s", mStatus.c_str());
    }
}

void ResourcesLayer::onUpdate()
{
    processPendingImports();
    if(mScannedRoot != fg::AssetRegistry::ResourcesRoot())
    {
        mNeedsRefresh = true;
    }
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

    drawToolbar();

    ImGui::SetNextItemWidth(-1.0f);
    char filterBuf[128] {};
    std::snprintf(filterBuf, sizeof(filterBuf), "%s", mFilter.c_str());
    if(ImGui::InputTextWithHint("##resource_filter", ICON_BTSP_SEARCH " Filter...", filterBuf,
                                sizeof(filterBuf)))
    {
        mFilter = filterBuf;
    }

    drawBreadcrumbs();
    ImGui::Separator();

    const float inspectorHeight = EditorUiScale::S(128.0f);
    if(ImGui::BeginChild("##resource_browser", ImVec2(0.0f, -inspectorHeight),
                         ImGuiChildFlags_Borders))
    {
        drawBrowser();
    }
    ImGui::EndChild();
    acceptHierarchyPrefabDrop(currentRelativePath());

    ImGui::Separator();
    drawInspector();

    ImGui::End();
}
