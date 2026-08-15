#include "Frigga/ECS/EcsLayout.hpp"

#define SIMDJSON_STATIC_REFLECTION 1
#include <simdjson.h>

#include <algorithm>
#include <fstream>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        constexpr int64_t kLayoutVersion = 1;

        struct EcsPipelineDto
        {
            std::string              name;
            float                    hz      = 0.0f;
            bool                     enabled = true;
            std::vector<std::string> systems;
        };

        struct EcsLayoutDto
        {
            int64_t                   version = kLayoutVersion;
            std::string               defaultPipeline {"Simulation"};
            std::vector<EcsPipelineDto> pipelines;
        };

        [[nodiscard]] bool LabelEndsWith(std::string_view label, std::string_view suffix)
        {
            return label.size() >= suffix.size() &&
                   label.substr(label.size() - suffix.size()) == suffix;
        }

        [[nodiscard]] std::optional<fr::SystemId> FindSystemByLabel(fr::Registry &registry,
                                                                    std::string_view label)
        {
            std::optional<fr::SystemId> found;
            registry.ForEachRegisteredSystem([&](fr::SystemId id, std::string_view name) {
                if(!found && name == label)
                {
                    found = id;
                }
            });
            return found;
        }
    } // namespace

    std::string ShortTypeLabel(std::string_view full)
    {
        const auto pos = full.rfind(':');
        if(pos == std::string_view::npos || pos + 1 >= full.size())
        {
            return std::string(full);
        }
        return std::string(full.substr(pos + 1));
    }

    bool IsBuiltinPipelineName(std::string_view name)
    {
        return name == kMainPipelineName || name == kDefaultEcsPipelineName ||
               name == kRenderPipelineName;
    }

    bool IsEngineSystemLabel(std::string_view label)
    {
        return LabelEndsWith(label, "AnimationSystem") || LabelEndsWith(label, "RenderSystem") ||
               LabelEndsWith(label, "PhysicsSystem") ||
               LabelEndsWith(label, "ThirdPersonCameraSystem") ||
               LabelEndsWith(label, "GameplayPluginBridge");
    }

    const char *EngineSystemBuiltinPipeline(std::string_view label)
    {
        if(LabelEndsWith(label, "RenderSystem"))
        {
            return kRenderPipelineName.data();
        }
        if(LabelEndsWith(label, "AnimationSystem") ||
           LabelEndsWith(label, "ThirdPersonCameraSystem"))
        {
            return kMainPipelineName.data();
        }
        return kDefaultEcsPipelineName.data();
    }

    void EnforceBuiltinGameLoop(fr::Registry &registry)
    {
        auto ensure = [&](std::string_view name, float hz) {
            auto id = registry.FindPipelineId(std::string(name));
            if(!id)
            {
                id = registry.RegisterPipeline(std::string(name), hz);
            }
            else
            {
                registry.SetPipelineRate(*id, hz);
            }
            return *id;
        };

        (void)ensure(kDefaultEcsPipelineName, kSimulationRateHz);
        (void)ensure(kMainPipelineName, 0.0f);
        const auto renderId = ensure(kRenderPipelineName, 0.0f);

        registry.ForEachRegisteredSystem([&](fr::SystemId id, std::string_view label) {
            if(!IsEngineSystemLabel(label))
            {
                return;
            }
            const auto targetName = EngineSystemBuiltinPipeline(label);
            const auto targetId   = registry.FindPipelineId(targetName);
            if(!targetId)
            {
                return;
            }
            const auto current = registry.FindPipelineContaining(id);
            if(current && *current == *targetId)
            {
                return;
            }
            const auto slot = registry.GetPipeline(*targetId).Systems.size();
            (void)registry.MoveSystem(id, *targetId, slot);
        });

        (void)registry.MovePipeline(renderId, static_cast<std::size_t>(registry.GetPipelineCount()));
    }

    float StoredRateToHz(float storedRate)
    {
        return storedRate > 1e-8f ? 1.0f / storedRate : 0.0f;
    }

    EcsLayout CaptureEcsLayout(fr::Registry &registry)
    {
        EcsLayout layout {};
        layout.version         = kLayoutVersion;
        layout.defaultPipeline = std::string(kDefaultEcsPipelineName);

        std::unordered_map<fr::SystemId, std::string> labels;
        registry.ForEachRegisteredSystem([&](fr::SystemId id, std::string_view name) {
            labels[id] = std::string(name);
        });

        registry.ForEachPipeline([&](const fr::PipelineView &pipeline) {
            EcsPipelineLayout row {};
            row.name    = std::string(pipeline.Name);
            row.hz      = pipeline.Name == kDefaultEcsPipelineName
                              ? kSimulationRateHz
                              : StoredRateToHz(pipeline.Rate);
            row.enabled = pipeline.Name == kDefaultEcsPipelineName ? true : pipeline.Enabled;
            row.systems.reserve(pipeline.Systems.size());
            for(const auto systemId : pipeline.Systems)
            {
                if(const auto found = labels.find(systemId); found != labels.end())
                {
                    row.systems.push_back(found->second);
                }
            }
            layout.pipelines.push_back(std::move(row));
        });
        return layout;
    }

    EcsLayoutApplyResult ApplyEcsLayout(fr::Registry &registry, const EcsLayout &layout)
    {
        EcsLayoutApplyResult result {};
        result.ok = true;

        const std::string defaultName =
            layout.defaultPipeline.empty() ? std::string(kDefaultEcsPipelineName)
                                           : layout.defaultPipeline;

        std::unordered_set<std::string> savedLabels;
        for(std::size_t i = 0; i < layout.pipelines.size(); ++i)
        {
            const auto &pipe = layout.pipelines[i];
            if(pipe.name.empty())
            {
                continue;
            }
            auto id = registry.FindPipelineId(pipe.name);
            if(!id)
            {
                id = registry.RegisterPipeline(pipe.name, pipe.hz, i);
            }
            if(!IsBuiltinPipelineName(pipe.name))
            {
                registry.SetPipelineRate(*id, pipe.hz);
                registry.SetPipelineEnabled(*id, pipe.enabled);
            }
            for(const auto &label : pipe.systems)
            {
                savedLabels.insert(label);
            }
        }

        for(std::size_t i = 0; i < layout.pipelines.size(); ++i)
        {
            const auto id = registry.FindPipelineId(layout.pipelines[i].name);
            if(id)
            {
                (void)registry.MovePipeline(*id, i);
            }
        }

        for(const auto &pipe : layout.pipelines)
        {
            const auto pipeId = registry.FindPipelineId(pipe.name);
            if(!pipeId)
            {
                continue;
            }
            std::size_t index = 0;
            for(const auto &label : pipe.systems)
            {
                const auto systemId = FindSystemByLabel(registry, label);
                if(!systemId)
                {
                    continue;
                }
                (void)registry.MoveSystem(*systemId, *pipeId, index);
                ++index;
            }
        }

        auto defaultId = registry.FindPipelineId(defaultName);
        if(!defaultId)
        {
            defaultId = registry.FindPipelineId(std::string(kDefaultEcsPipelineName));
        }

        if(defaultId)
        {
            std::vector<fr::SystemId> newcomers;
            registry.ForEachRegisteredSystem([&](fr::SystemId id, std::string_view label) {
                if(savedLabels.contains(std::string(label)) || IsEngineSystemLabel(label))
                {
                    return;
                }
                newcomers.push_back(id);
            });
            auto slot = registry.GetPipeline(*defaultId).Systems.size();
            for(const auto id : newcomers)
            {
                (void)registry.MoveSystem(id, *defaultId, slot);
                ++slot;
                result.addedPluginSystems = true;
            }
        }

        EnforceBuiltinGameLoop(registry);
        return result;
    }

    bool ParseEcsLayout(std::string_view json, EcsLayout &out, std::string *error)
    {
        const simdjson::padded_string padded(json);
        EcsLayoutDto document {};
        if(const auto err = simdjson::from(padded).get(document); err)
        {
            if(error)
            {
                *error = simdjson::error_message(err);
            }
            return false;
        }
        out.version         = document.version;
        out.defaultPipeline = document.defaultPipeline.empty()
                                  ? std::string(kDefaultEcsPipelineName)
                                  : document.defaultPipeline;
        out.pipelines.clear();
        out.pipelines.reserve(document.pipelines.size());
        for(const auto &pipe : document.pipelines)
        {
            out.pipelines.push_back(EcsPipelineLayout {
                .name     = pipe.name,
                .hz       = pipe.hz,
                .enabled  = pipe.enabled,
                .systems  = pipe.systems,
            });
        }
        return true;
    }

    bool SerializeEcsLayout(const EcsLayout &layout, std::string &outJson, std::string *error)
    {
        EcsLayoutDto document {};
        document.version         = layout.version <= 0 ? kLayoutVersion : layout.version;
        document.defaultPipeline = layout.defaultPipeline.empty()
                                       ? std::string(kDefaultEcsPipelineName)
                                       : layout.defaultPipeline;
        document.pipelines.reserve(layout.pipelines.size());
        for(const auto &pipe : layout.pipelines)
        {
            document.pipelines.push_back(EcsPipelineDto {
                .name     = pipe.name,
                .hz       = pipe.hz,
                .enabled  = pipe.enabled,
                .systems  = pipe.systems,
            });
        }
        outJson.clear();
        if(const auto err = simdjson::to_json(document, outJson); err)
        {
            if(error)
            {
                *error = simdjson::error_message(err);
            }
            return false;
        }
        return true;
    }

    bool LoadEcsLayoutFile(const std::filesystem::path &path, EcsLayout &out, std::string *error)
    {
        simdjson::padded_string json;
        if(const auto err = simdjson::padded_string::load(path.string()).get(json); err)
        {
            if(error)
            {
                *error = simdjson::error_message(err);
            }
            return false;
        }
        return ParseEcsLayout(std::string_view(json.data(), json.size()), out, error);
    }

    bool SaveEcsLayoutFile(const std::filesystem::path &path, const EcsLayout &layout,
                           std::string *error)
    {
        std::string json;
        if(!SerializeEcsLayout(layout, json, error))
        {
            return false;
        }
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if(!file)
        {
            if(error)
            {
                *error = "Failed to open ecs.json for writing";
            }
            return false;
        }
        file << json;
        if(!json.empty() && json.back() != '\n')
        {
            file << '\n';
        }
        return static_cast<bool>(file);
    }

    EcsLayoutApplyResult SyncEcsLayoutFile(fr::Registry &registry,
                                           const std::filesystem::path &path)
    {
        EcsLayoutApplyResult result {};
        std::error_code ec;
        if(!std::filesystem::exists(path, ec))
        {
            EnforceBuiltinGameLoop(registry);
            const auto captured = CaptureEcsLayout(registry);
            if(!SaveEcsLayoutFile(path, captured, &result.error))
            {
                return result;
            }
            result.ok = true;
            return result;
        }

        EcsLayout layout {};
        if(!LoadEcsLayoutFile(path, layout, &result.error))
        {
            return result;
        }
        result = ApplyEcsLayout(registry, layout);
        if(!result.ok)
        {
            return result;
        }
        const auto captured = CaptureEcsLayout(registry);
        if(!SaveEcsLayoutFile(path, captured, &result.error))
        {
            result.ok = false;
        }
        return result;
    }

} // namespace FRIGGA_NAMESPACE
