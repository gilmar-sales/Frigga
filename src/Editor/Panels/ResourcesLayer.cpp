#include "ResourcesLayer.hpp"

#include "Editor/DockLayout.hpp"
#include "Frigga/ECS/Components/AnimatorComponent.hpp"
#include "Frigga/ECS/Components/MaterialComponent.hpp"
#include "Frigga/ECS/Components/MeshComponent.hpp"
#include "Frigga/ECS/Components/NameComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"

#include <SDL3/SDL_dialog.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <format>
#include <imgui.h>

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
        if(name == "Models")
        {
            return 1;
        }
        if(name == "Textures")
        {
            return 2;
        }
        if(name == "Materials")
        {
            return 3;
        }
        if(name == "Environments")
        {
            return 4;
        }
        if(name == "Shaders")
        {
            return 100;
        }
        return 50;
    }
} // namespace

ResourcesLayer::ResourcesLayer(skr::Arc<fg::PrimitiveMeshFactory> primitives,
                               skr::Arc<fg::AssetRegistry> assets, skr::Arc<fr::Registry> registry,
                               skr::Arc<SelectionContext> selection,
                               skr::Arc<fg::SceneSimulationState> simulation,
                               skr::Arc<fra::Window> window)
    : fg::Layer("Resources"), mPrimitives(std::move(primitives)), mAssets(std::move(assets)),
      mRegistry(std::move(registry)), mSelection(std::move(selection)),
      mSimulation(std::move(simulation)), mWindow(std::move(window))
{
}

bool ResourcesLayer::passesFilter(std::string_view text) const
{
    return ContainsIgnoreCase(text, mFilter);
}

void ResourcesLayer::refresh()
{
    mRoot         = AssetFolder {.name = kResourcesRoot};
    mNeedsRefresh = false;

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

        folder.entries.push_back(std::move(asset));
    }

    std::ranges::sort(folder.entries,
                      [](const AssetEntry &a, const AssetEntry &b) { return a.label < b.label; });
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

    switch(entry.kind)
    {
    case EntryKind::Primitive:
        return entry.primitive == mSelected->primitive;
    case EntryKind::Material:
        return entry.materialId == mSelected->materialId;
    case EntryKind::Model:
    case EntryKind::Texture:
    case EntryKind::File:
        return entry.relativePath == mSelected->relativePath;
    }
    return false;
}

void ResourcesLayer::selectEntry(const AssetEntry &entry)
{
    mSelected = entry;
}

void ResourcesLayer::drawEntry(const AssetEntry &entry)
{
    if(!passesFilter(entry.label) &&
       !((entry.kind == EntryKind::File || entry.kind == EntryKind::Model ||
          entry.kind == EntryKind::Texture) &&
         passesFilter(entry.relativePath.string())))
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
    if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        selectEntry(entry);
        spawnSelected();
    }

    if(ImGui::BeginPopupContextItem())
    {
        selectEntry(entry);
        const bool canSpawn =
            entry.kind == EntryKind::Primitive || entry.kind == EntryKind::Model;
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
}

void ResourcesLayer::drawFolder(const AssetFolder &folder)
{
    bool anyVisible = mFilter.empty();
    if(!anyVisible)
    {
        for(const auto &entry : folder.entries)
        {
            if(passesFilter(entry.label) ||
               ((entry.kind == EntryKind::File || entry.kind == EntryKind::Model ||
                 entry.kind == EntryKind::Texture) &&
                passesFilter(entry.relativePath.string())))
            {
                anyVisible = true;
                break;
            }
        }
        if(!anyVisible)
        {
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
            mStatus = std::format("Imported model '{}'", model->relativePath);
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
            mStatus = std::format("Imported texture '{}'", texture->relativePath);
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
    if(ImGui::Button("Import Model"))
    {
        requestImportModel();
    }
    ImGui::SameLine();
    if(ImGui::Button("Import Texture"))
    {
        requestImportTexture();
    }
    ImGui::SameLine();
    if(ImGui::Button("New Material"))
    {
        createMaterialAsset();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if(ImGui::Button("Refresh"))
    {
        mNeedsRefresh = true;
    }
}

void ResourcesLayer::drawInspector()
{
    if(!mSelected)
    {
        ImGui::TextDisabled("Select an asset to inspect.");
        return;
    }

    switch(mSelected->kind)
    {
    case EntryKind::Primitive:
        ImGui::TextUnformatted("Primitive mesh");
        ImGui::Text("Name: %s", mSelected->label.c_str());
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
        ImGui::TextUnformatted("Model");
        ImGui::Text("Name: %s", mSelected->label.c_str());
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
        ImGui::TextUnformatted("Texture");
        ImGui::Text("Name: %s", mSelected->label.c_str());
        ImGui::TextWrapped("Path: Resources/%s", mSelected->relativePath.string().c_str());
        if(const auto texture = mAssets->LoadTexture(mSelected->relativePath))
        {
            mSelected->textureId = texture->textureId;
            ImGui::Text("Texture ID: %u", texture->textureId);
        }
        break;
    }
    case EntryKind::Material:
        ImGui::TextUnformatted("Material");
        ImGui::Text("Name: %s", mSelected->label.c_str());
        ImGui::Text("Material ID: %u", mSelected->materialId);
        ImGui::BeginDisabled(mSimulation->IsPlaying() ||
                             mSelection->Get() == SelectionContext::Invalid);
        if(ImGui::Button("Assign to Selection"))
        {
            assignMaterialToSelection(mSelected->materialId);
        }
        ImGui::EndDisabled();
        break;
    case EntryKind::File:
        ImGui::TextUnformatted("File asset");
        ImGui::Text("Name: %s", mSelected->label.c_str());
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
    if(ImGui::InputTextWithHint("##resource_filter", "Filter...", filterBuf, sizeof(filterBuf)))
    {
        mFilter = filterBuf;
    }

    ImGui::Separator();

    if(ImGui::BeginChild("##resource_browser", ImVec2(0.0f, -120.0f), ImGuiChildFlags_Borders))
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
    drawInspector();

    ImGui::End();
}
