#include "ArchetypesLayer.hpp"

#include "Editor/DockLayout.hpp"
#include "Editor/UiScale.hpp"
#include "Frigga/ECS/Components/NameComponent.hpp"
#include "Frigga/ECS/EcsLayout.hpp"

#include <Freyr/Containers/Archetype.hpp>

#include <format>
#include <imgui.h>
#include <string>
#include <vector>

ArchetypesLayer::ArchetypesLayer(skr::Arc<fr::Registry> registry,
                                 skr::Arc<SelectionContext> selection)
    : fg::Layer("Archetypes"), mRegistry(std::move(registry)), mSelection(std::move(selection))
{
}

void ArchetypesLayer::onGui()
{
    const auto title = EditorDock::WindowId("Archetypes");
    ImGui::Begin(title.c_str());

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##ArchetypeFilter", "Filter archetypes…", mFilter, sizeof(mFilter));

    const std::string filter = mFilter;
    std::size_t totalEntities = 0;
    std::size_t visible       = 0;

    if(ImGui::BeginTable("##Archetypes", 4,
                         ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                             ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY))
    {
        ImGui::TableSetupColumn("Archetype", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Entities", ImGuiTableColumnFlags_WidthFixed,
                                EditorUiScale::S(80.0f));
        ImGui::TableSetupColumn("Chunks", ImGuiTableColumnFlags_WidthFixed,
                                EditorUiScale::S(70.0f));
        ImGui::TableSetupColumn("Components", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        mRegistry->ForEachArchetype([&](fr::Archetype *archetype) {
            if(archetype == nullptr)
            {
                return;
            }
            totalEntities += archetype->Count();
            std::string name(archetype->GetName());
            if(!filter.empty() && name.find(filter) == std::string::npos)
            {
                return;
            }
            ++visible;

            std::vector<std::string> components;
            archetype->ForEachComponent([&](fr::ComponentId, std::string_view typeName) {
                components.emplace_back(fg::ShortTypeLabel(typeName));
            });

            bool containsSelection = false;
            const auto selected    = mSelection->Get();
            if(mSelection->HasSelection())
            {
                archetype->ForEachChunk([&](fr::ArchetypeChunk *chunk) {
                    if(containsSelection || chunk == nullptr)
                    {
                        return;
                    }
                    for(std::size_t i = 0; i < chunk->Count(); ++i)
                    {
                        if(chunk->GetEntityAt(i) == selected)
                        {
                            containsSelection = true;
                            break;
                        }
                    }
                });
            }

            ImGui::TableNextRow();
            if(containsSelection)
            {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                       ImGui::GetColorU32(ImGuiCol_Header));
            }

            ImGui::TableSetColumnIndex(0);
            const bool open = ImGui::TreeNodeEx(
                reinterpret_cast<void *>(archetype),
                ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap,
                "%s", name.c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%zu", archetype->Count());
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%zu", archetype->ChunkCount());
            ImGui::TableSetColumnIndex(3);
            std::string joined;
            for(std::size_t i = 0; i < components.size(); ++i)
            {
                if(i != 0)
                {
                    joined += ", ";
                }
                joined += components[i];
            }
            ImGui::TextUnformatted(joined.c_str());

            if(open)
            {
                archetype->ForEachChunk([&](fr::ArchetypeChunk *chunk) {
                    if(chunk == nullptr)
                    {
                        return;
                    }
                    for(std::size_t i = 0; i < chunk->Count(); ++i)
                    {
                        const auto entity = chunk->GetEntityAt(i);
                        std::string label = std::format("Entity {}", entity);
                        mRegistry->TryGetComponents<fg::NameComponent>(
                            entity, [&](fg::NameComponent &name) { label = name.name; });
                        const bool selectedRow = mSelection->Get() == entity;
                        if(ImGui::Selectable(std::format("{}##{}", label, entity).c_str(),
                                             selectedRow))
                        {
                            mSelection->Select(entity);
                        }
                    }
                });
                ImGui::TreePop();
            }
        });
        ImGui::EndTable();
    }

    ImGui::TextDisabled("%zu archetypes · %zu entities", mRegistry->ArchetypeCount(),
                        totalEntities);
    if(!filter.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(%zu shown)", visible);
    }

    ImGui::End();
}
