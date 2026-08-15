#include "PipelinesLayer.hpp"

#include "Editor/DockLayout.hpp"
#include "Frigga/ECS/EcsLayout.hpp"
#include "Frigga/ECS/Systems/AnimationSystem.hpp"
#include "Frigga/ECS/Systems/PhysicsSystem.hpp"
#include "Frigga/ECS/Systems/RenderSystem.hpp"
#include "Frigga/ECS/Systems/ThirdPersonCameraSystem.hpp"
#include "Frigga/Plugin/GameplayPluginBridge.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <format>
#include <unordered_map>
#include <vector>

namespace
{
    constexpr const char *kPipelinePayload = "FRIGGA_ECS_PIPELINE";
    constexpr const char *kSystemPayload   = "FRIGGA_ECS_SYSTEM";

    struct SystemDrag
    {
        fr::SystemId systemId   = 0;
        int32_t      pipelineId = -1;
        std::size_t  index      = 0;
    };

    struct EngineCatalogEntry
    {
        const char *label          = nullptr;
        const char *defaultPipeline = nullptr;
        bool (*isRegistered)(fr::Registry &) = nullptr;
        void (*registerInto)(fr::Registry &, int32_t) = nullptr;
    };

    template <typename T>
    bool IsRegistered(fr::Registry &registry)
    {
        return registry.IsSystemRegistered<T>();
    }

    template <typename T>
    void RegisterInto(fr::Registry &registry, int32_t pipelineId)
    {
        registry.RegisterSystem<T>(pipelineId);
    }

    const EngineCatalogEntry kEngineCatalog[] = {
        {"AnimationSystem", "Main", &IsRegistered<fg::AnimationSystem>,
         &RegisterInto<fg::AnimationSystem>},
        {"RenderSystem", "Main", &IsRegistered<fg::RenderSystem>, &RegisterInto<fg::RenderSystem>},
        {"GameplayPluginBridge", "Simulation", &IsRegistered<fg::GameplayPluginBridge>,
         &RegisterInto<fg::GameplayPluginBridge>},
        {"PhysicsSystem", "Simulation", &IsRegistered<fg::PhysicsSystem>,
         &RegisterInto<fg::PhysicsSystem>},
        {"ThirdPersonCameraSystem", "Simulation", &IsRegistered<fg::ThirdPersonCameraSystem>,
         &RegisterInto<fg::ThirdPersonCameraSystem>},
    };

    [[nodiscard]] bool AcceptPipelineDrop(int32_t &outId)
    {
        if(const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(kPipelinePayload))
        {
            if(payload->DataSize == sizeof(int32_t))
            {
                outId = *static_cast<const int32_t *>(payload->Data);
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool AcceptSystemDrop(SystemDrag &out)
    {
        if(const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(kSystemPayload))
        {
            if(payload->DataSize == sizeof(SystemDrag))
            {
                out = *static_cast<const SystemDrag *>(payload->Data);
                return true;
            }
        }
        return false;
    }
} // namespace

PipelinesLayer::PipelinesLayer(skr::Arc<fr::Registry> registry, skr::Arc<ProjectSession> session,
                               skr::Arc<fg::SceneSimulationState> simulation)
    : fg::Layer("Pipelines"), mRegistry(std::move(registry)), mSession(std::move(session)),
      mSimulation(std::move(simulation))
{
}

void PipelinesLayer::persistLayout()
{
    mError.clear();
    if(!mSession->SaveEcsLayout())
    {
        const auto err = mSession->GetLastError();
        mError = err.empty() ? "Failed to save ecs.json" : err;
        return;
    }
    mStatus = "Saved ecs.json";
}

void PipelinesLayer::addPipeline()
{
    mError.clear();
    std::string name = mNewName;
    while(!name.empty() && (name.front() == ' ' || name.front() == '\t'))
    {
        name.erase(name.begin());
    }
    while(!name.empty() && (name.back() == ' ' || name.back() == '\t'))
    {
        name.pop_back();
    }
    if(name.empty())
    {
        mError = "Pipeline name cannot be empty";
        return;
    }
    if(mRegistry->FindPipelineId(name))
    {
        mError = "A pipeline with that name already exists";
        return;
    }
    (void)mRegistry->RegisterPipeline(name, mNewHz);
    std::memset(mNewName, 0, sizeof(mNewName));
    mNewHz = 0.0f;
    persistLayout();
}

void PipelinesLayer::deleteUserPipeline(int32_t pipelineId)
{
    const auto view = mRegistry->GetPipeline(pipelineId);
    if(fg::IsBuiltinPipelineName(view.Name))
    {
        return;
    }

    const auto simulationId = mRegistry->FindPipelineId(std::string(fg::kDefaultEcsPipelineName));
    if(!simulationId)
    {
        mError = "Simulation pipeline is missing; cannot rehome systems";
        return;
    }

    const std::vector<fr::SystemId> systems(view.Systems.begin(), view.Systems.end());
    auto slot = mRegistry->GetPipeline(*simulationId).Systems.size();
    for(const auto systemId : systems)
    {
        (void)mRegistry->MoveSystem(systemId, *simulationId, slot);
        ++slot;
    }
    (void)mRegistry->UnregisterPipeline(pipelineId);
    persistLayout();
}

void PipelinesLayer::registerEngineSystem(std::size_t catalogIndex, int32_t pipelineId)
{
    if(catalogIndex >= std::size(kEngineCatalog))
    {
        return;
    }
    auto &entry = kEngineCatalog[catalogIndex];
    if(entry.isRegistered(*mRegistry))
    {
        mError = std::format("{} is already registered", entry.label);
        return;
    }
    entry.registerInto(*mRegistry, pipelineId);
    persistLayout();
}

void PipelinesLayer::drawSystemRow(fr::SystemId systemId, std::string_view label, int32_t pipelineId,
                                   std::size_t index, std::size_t count)
{
    ImGui::PushID(static_cast<int>(systemId));
    const std::string shortLabel = fg::ShortTypeLabel(label);

    ImGui::Selectable(shortLabel.c_str(), false, ImGuiSelectableFlags_SpanAllColumns);
    if(ImGui::BeginDragDropSource())
    {
        SystemDrag drag {systemId, pipelineId, index};
        ImGui::SetDragDropPayload(kSystemPayload, &drag, sizeof(drag));
        ImGui::TextUnformatted(shortLabel.c_str());
        ImGui::EndDragDropSource();
    }
    if(ImGui::BeginDragDropTarget())
    {
        SystemDrag drag {};
        if(AcceptSystemDrop(drag))
        {
            (void)mRegistry->MoveSystem(drag.systemId, pipelineId, index);
            persistLayout();
        }
        ImGui::EndDragDropTarget();
    }
    if(ImGui::BeginPopupContextItem("##SystemMenu"))
    {
        if(ImGui::MenuItem("Remove system"))
        {
            (void)mRegistry->UnregisterSystem(systemId);
            persistLayout();
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(index == 0);
    if(ImGui::SmallButton("^"))
    {
        (void)mRegistry->MoveSystem(systemId, pipelineId, index - 1);
        persistLayout();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(index + 1 >= count);
    if(ImGui::SmallButton("v"))
    {
        (void)mRegistry->MoveSystem(systemId, pipelineId, index + 1);
        persistLayout();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    if(ImGui::BeginCombo("##MoveTo", "Move to…"))
    {
        mRegistry->ForEachPipeline([&](const fr::PipelineView &pipe) {
            if(pipe.Id == pipelineId)
            {
                return;
            }
            if(ImGui::Selectable(std::string(pipe.Name).c_str()))
            {
                (void)mRegistry->MoveSystem(systemId, pipe.Id, pipe.Systems.size());
                persistLayout();
            }
        });
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    if(ImGui::SmallButton("Remove"))
    {
        (void)mRegistry->UnregisterSystem(systemId);
        persistLayout();
    }

    ImGui::PopID();
}

void PipelinesLayer::drawPipeline(int32_t pipelineId, std::string_view nameView, float storedRate,
                                  bool enabled, const std::vector<fr::SystemId> &systems,
                                  std::size_t index, std::size_t count)
{
    const std::string name(nameView);
    const bool builtin      = fg::IsBuiltinPipelineName(name);
    const bool isMain       = name == fg::kMainPipelineName;
    const bool isSimulation = name == fg::kDefaultEcsPipelineName;

    ImGui::PushID(pipelineId);
    const bool open = ImGui::TreeNodeEx("##pipe", ImGuiTreeNodeFlags_DefaultOpen |
                                                      ImGuiTreeNodeFlags_SpanAvailWidth,
                                        "%s", name.c_str());
    if(ImGui::BeginDragDropSource())
    {
        ImGui::SetDragDropPayload(kPipelinePayload, &pipelineId, sizeof(pipelineId));
        ImGui::TextUnformatted(name.c_str());
        ImGui::EndDragDropSource();
    }
    if(ImGui::BeginDragDropTarget())
    {
        int32_t droppedPipe = -1;
        if(AcceptPipelineDrop(droppedPipe) && droppedPipe != pipelineId)
        {
            (void)mRegistry->MovePipeline(droppedPipe, index);
            persistLayout();
        }
        SystemDrag drag {};
        if(AcceptSystemDrop(drag))
        {
            (void)mRegistry->MoveSystem(drag.systemId, pipelineId, systems.size());
            persistLayout();
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(index == 0);
    if(ImGui::SmallButton("^"))
    {
        (void)mRegistry->MovePipeline(pipelineId, index - 1);
        persistLayout();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(index + 1 >= count);
    if(ImGui::SmallButton("v"))
    {
        (void)mRegistry->MovePipeline(pipelineId, index + 1);
        persistLayout();
    }
    ImGui::EndDisabled();

    if(!builtin)
    {
        ImGui::SameLine();
        if(ImGui::SmallButton("Delete"))
        {
            mPendingDeleteId = pipelineId;
            ImGui::OpenPopup("Delete pipeline?");
        }
    }

    if(open)
    {
        ImGui::Indent();
        if(!builtin)
        {
            char nameBuf[64];
            std::snprintf(nameBuf, sizeof(nameBuf), "%s", name.c_str());
            ImGui::InputText("Name", nameBuf, sizeof(nameBuf));
            if(ImGui::IsItemDeactivatedAfterEdit())
            {
                const std::string next(nameBuf);
                if(next.empty() || fg::IsBuiltinPipelineName(next))
                {
                    mError = "Invalid pipeline name";
                }
                else if(next != name && mRegistry->FindPipelineId(next))
                {
                    mError = "A pipeline with that name already exists";
                }
                else if(next != name)
                {
                    mRegistry->SetPipelineName(pipelineId, next);
                    persistLayout();
                }
            }
        }
        else
        {
            ImGui::BeginDisabled(true);
            ImGui::Text("Name: %s", name.c_str());
            ImGui::EndDisabled();
        }

        float hz = fg::StoredRateToHz(storedRate);
        if(ImGui::DragFloat("Hz", &hz, 1.0f, 0.0f, 1000.0f, hz <= 0.0f ? "every frame" : "%.1f"))
        {
            mRegistry->SetPipelineRate(pipelineId, hz);
        }
        if(ImGui::IsItemDeactivatedAfterEdit())
        {
            persistLayout();
        }
        if(isMain)
        {
            ImGui::BeginDisabled(true);
            enabled = true;
            ImGui::Checkbox("Enabled", &enabled);
            ImGui::EndDisabled();
            if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::SetTooltip("Main cannot be disabled");
            }
        }
        else if(isSimulation)
        {
            ImGui::BeginDisabled(true);
            enabled = mSimulation->IsPlaying();
            ImGui::Checkbox("Enabled (Play)", &enabled);
            ImGui::EndDisabled();
            if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::SetTooltip("Simulation enable is driven by Play / Stop");
            }
        }
        else
        {
            if(ImGui::Checkbox("Enabled", &enabled))
            {
                mRegistry->SetPipelineEnabled(pipelineId, enabled);
                persistLayout();
            }
        }

        ImGui::TextDisabled("Systems");
        std::unordered_map<fr::SystemId, std::string> labels;
        mRegistry->ForEachRegisteredSystem([&](fr::SystemId id, std::string_view systemName) {
            labels[id] = std::string(systemName);
        });
        for(std::size_t i = 0; i < systems.size(); ++i)
        {
            const auto found = labels.find(systems[i]);
            drawSystemRow(systems[i], found == labels.end() ? "?" : found->second, pipelineId, i,
                          systems.size());
        }

        if(ImGui::BeginCombo("Add engine system", "Add engine system…"))
        {
            bool any = false;
            for(std::size_t i = 0; i < std::size(kEngineCatalog); ++i)
            {
                if(kEngineCatalog[i].isRegistered(*mRegistry))
                {
                    continue;
                }
                any = true;
                if(ImGui::Selectable(kEngineCatalog[i].label))
                {
                    registerEngineSystem(i, pipelineId);
                }
            }
            if(!any)
            {
                ImGui::TextDisabled("All engine systems are registered");
            }
            ImGui::EndCombo();
        }

        ImGui::Unindent();
        ImGui::TreePop();
    }

    if(ImGui::BeginPopupModal("Delete pipeline?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Move systems to Simulation and delete '%s'?", name.c_str());
        if(ImGui::Button("Delete"))
        {
            deleteUserPipeline(pipelineId);
            mPendingDeleteId = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if(ImGui::Button("Cancel"))
        {
            mPendingDeleteId = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::PopID();
}

void PipelinesLayer::onGui()
{
    const auto title = EditorDock::WindowId("Pipelines");
    ImGui::Begin(title.c_str());

    ImGui::TextUnformatted("Tick order (top to bottom)");
    ImGui::SameLine();
    ImGui::TextDisabled("— drag to reorder");

    std::vector<int32_t> pipelineIds;
    mRegistry->ForEachPipeline([&](const fr::PipelineView &pipeline) {
        pipelineIds.push_back(pipeline.Id);
    });

    for(std::size_t i = 0; i < pipelineIds.size(); ++i)
    {
        if(!mRegistry->HasPipeline(pipelineIds[i]))
        {
            continue;
        }
        const auto view = mRegistry->GetPipeline(pipelineIds[i]);
        const std::vector<fr::SystemId> systems(view.Systems.begin(), view.Systems.end());
        drawPipeline(view.Id, view.Name, view.Rate, view.Enabled, systems, i, pipelineIds.size());
        ImGui::Separator();
    }

    ImGui::SeparatorText("Add pipeline");
    ImGui::SetNextItemWidth(180.0f);
    ImGui::InputText("##NewPipeline", mNewName, sizeof(mNewName));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    ImGui::DragFloat("Hz##New", &mNewHz, 1.0f, 0.0f, 1000.0f,
                     mNewHz <= 0.0f ? "every frame" : "%.1f");
    ImGui::SameLine();
    if(ImGui::Button("Add"))
    {
        addPipeline();
    }

    if(!mError.empty())
    {
        ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s", mError.c_str());
    }
    else if(!mStatus.empty())
    {
        ImGui::TextDisabled("%s", mStatus.c_str());
    }

    ImGui::End();
}
