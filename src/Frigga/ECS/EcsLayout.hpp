#pragma once

#include <Freyr/Freyr.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace FRIGGA_NAMESPACE
{

    inline constexpr std::string_view kEcsLayoutFileName      = "ecs.json";
    inline constexpr std::string_view kDefaultEcsPipelineName = "Simulation";
    inline constexpr std::string_view kMainPipelineName       = "Main";
    inline constexpr std::string_view kRenderPipelineName     = "Render";
    inline constexpr float            kSimulationRateHz       = 60.0f;

    struct EcsPipelineLayout
    {
        std::string              name;
        float                    hz      = 0.0f;
        bool                     enabled = true;
        std::vector<std::string> systems;
    };

    struct EcsLayout
    {
        int64_t                       version = 1;
        std::string                   defaultPipeline {kDefaultEcsPipelineName};
        std::vector<EcsPipelineLayout> pipelines;
    };

    struct EcsLayoutApplyResult
    {
        bool ok                   = false;
        bool addedModuleSystems   = false;
        std::string error;
    };

    [[nodiscard]] std::string ShortTypeLabel(std::string_view full);
    [[nodiscard]] bool        IsBuiltinPipelineName(std::string_view name);
    [[nodiscard]] bool        IsEngineSystemLabel(std::string_view label);
    [[nodiscard]] const char *EngineSystemBuiltinPipeline(std::string_view label);
    void                      EnforceBuiltinGameLoop(fr::Registry &registry);

    [[nodiscard]] float StoredRateToHz(float storedRate);
    [[nodiscard]] EcsLayout CaptureEcsLayout(fr::Registry &registry);

    EcsLayoutApplyResult ApplyEcsLayout(fr::Registry &registry, const EcsLayout &layout);

    [[nodiscard]] bool ParseEcsLayout(std::string_view json, EcsLayout &out, std::string *error = nullptr);
    [[nodiscard]] bool SerializeEcsLayout(const EcsLayout &layout, std::string &outJson,
                                          std::string *error = nullptr);
    [[nodiscard]] bool LoadEcsLayoutFile(const std::filesystem::path &path, EcsLayout &out,
                                         std::string *error = nullptr);
    [[nodiscard]] bool SaveEcsLayoutFile(const std::filesystem::path &path, const EcsLayout &layout,
                                         std::string *error = nullptr);

    /// Load ecs.json if present, apply, and rewrite the file when new plugin systems appeared.
    /// Missing file: capture live runtime (after plugin attach) and write it.
    EcsLayoutApplyResult SyncEcsLayoutFile(fr::Registry &registry, const std::filesystem::path &path);

} // namespace FRIGGA_NAMESPACE
