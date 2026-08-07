#include "PreferencesLayer.hpp"

#include "../Preferences/PreferencesStore.hpp"

#include <Frigga/Gui/GuiLayer.hpp>
#include <Frigga/Gui/Styles/Styles.hpp>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstring>

bool PreferencesLayer::IsOpen = false;

namespace
{
    int SampleCountToIndex(std::uint32_t samples)
    {
        switch(samples)
        {
        case 2:
            return 1;
        case 4:
            return 2;
        case 8:
            return 3;
        default:
            return 0;
        }
    }

    std::uint32_t IndexToSampleCount(int index)
    {
        constexpr std::array<std::uint32_t, 4> kSamples {1, 2, 4, 8};
        const auto clamped = std::clamp(index, 0, 3);
        return kSamples[static_cast<std::size_t>(clamped)];
    }

    void RestartHint()
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(restart)");
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Saved to preferences.json. Requires editor restart.");
        }
    }

    void ActiveU32(const char *label, std::uint32_t value)
    {
        ImGui::TextDisabled("Active %s: %u", label, value);
    }

    void ActiveBool(const char *label, bool value)
    {
        ImGui::TextDisabled("Active %s: %s", label, value ? "on" : "off");
    }

    void ActiveFloat(const char *label, float value)
    {
        ImGui::TextDisabled("Active %s: %.4g", label, value);
    }

    void ActivePath(const char *label, const std::string &value)
    {
        ImGui::TextDisabled("Active %s: %s", label, value.c_str());
    }
} // namespace

void PreferencesLayer::persist()
{
    PreferencesStore::Save(*mPreferences);
}

void PreferencesLayer::syncShadowPrefsFromOptions()
{
    auto &prefs                         = mPreferences->graphics;
    const auto &o                       = *mFreyaOptions;
    prefs.shadowCascadeCount            = o.shadowCascadeCount;
    prefs.shadowMapResolution           = o.shadowMapResolution;
    prefs.maxSpotShadows                = o.maxSpotShadows;
    prefs.maxPointShadows               = o.maxPointShadows;
    prefs.shadowSampleCount             = o.shadowSampleCount;
    prefs.shadowQuality =
        static_cast<int>(mRenderer->GetShadowQuality());
}

void PreferencesLayer::applyTheme(int themeIndex) const
{
    switch(themeIndex)
    {
    case 0:
        fg::StylePhantomDark();
        break;
    case 1:
        fg::StylePhantomLight();
        break;
    case 2:
        ImGui::StyleColorsDark();
        break;
    case 3:
        ImGui::StyleColorsLight();
        break;
    case 4:
        ImGui::StyleColorsClassic();
        break;
    default:
        fg::StylePhantomDark();
        break;
    }
}

void PreferencesLayer::applyPendingGraphics()
{
    bool recreatePipeline = false;

    if(mPendingGraphics.vSync.has_value())
    {
        mRenderer->SetVSync(*mPendingGraphics.vSync);
        mPendingGraphics.vSync.reset();
        recreatePipeline = true;
    }
    if(mPendingGraphics.sampleCount.has_value())
    {
        mRenderer->SetSamples(*mPendingGraphics.sampleCount);
        mPendingGraphics.sampleCount.reset();
        recreatePipeline = true;
    }
    if(mPendingGraphics.shadowQuality.has_value())
    {
        mRenderer->SetShadowQuality(*mPendingGraphics.shadowQuality);
        mPendingGraphics.shadowQuality.reset();
        syncShadowPrefsFromOptions();
        persist();
        recreatePipeline = true;
    }

    if(recreatePipeline)
    {
        fg::GuiLayer::RecreateMainPipeline(mRenderer);
    }
}

void PreferencesLayer::onUpdate()
{
    applyPendingGraphics();
}

void PreferencesLayer::drawGraphicsTab()
{
    auto &prefs = mPreferences->graphics;

    ImGui::SeparatorText("Display");

    bool vSync = mPendingGraphics.vSync.value_or(mRenderer->GetVSync());
    if(ImGui::Checkbox("VSync", &vSync))
    {
        mPendingGraphics.vSync = vSync;
        prefs.vSync            = vSync;
        persist();
    }

    bool fullscreen = mWindow->IsFullscreen();
    if(ImGui::Checkbox("Fullscreen", &fullscreen))
    {
        mWindow->SetFullscreen(fullscreen);
        prefs.fullscreen = fullscreen;
        persist();
    }

    int sampleIndex = SampleCountToIndex(
        mPendingGraphics.sampleCount.value_or(mRenderer->GetSamples()));
    if(ImGui::Combo("MSAA", &sampleIndex, "1x\02x\04x\08x\0"))
    {
        const auto samples           = IndexToSampleCount(sampleIndex);
        mPendingGraphics.sampleCount = samples;
        prefs.sampleCount            = samples;
        persist();
    }

    int width = static_cast<int>(prefs.width);
    if(ImGui::InputInt("Width", &width))
    {
        prefs.width = static_cast<std::uint32_t>(std::max(1, width));
        persist();
    }
    RestartHint();
    ActiveU32("width", mFreyaOptions->width);

    int height = static_cast<int>(prefs.height);
    if(ImGui::InputInt("Height", &height))
    {
        prefs.height = static_cast<std::uint32_t>(std::max(1, height));
        persist();
    }
    RestartHint();
    ActiveU32("height", mFreyaOptions->height);

    char title[256] {};
    std::strncpy(title, prefs.title.c_str(), sizeof(title) - 1);
    if(ImGui::InputText("Title", title, sizeof(title)))
    {
        prefs.title = title;
        persist();
    }
    RestartHint();
    ActivePath("title", mFreyaOptions->title);

    int frameCount = static_cast<int>(prefs.frameCount);
    if(ImGui::InputInt("Frame Count", &frameCount))
    {
        prefs.frameCount = static_cast<std::uint32_t>(std::max(1, frameCount));
        persist();
    }
    RestartHint();
    ActiveU32("frameCount", mFreyaOptions->frameCount);

    float clearColor[4] = {
        static_cast<float>(prefs.clearColorR),
        static_cast<float>(prefs.clearColorG),
        static_cast<float>(prefs.clearColorB),
        static_cast<float>(prefs.clearColorA),
    };
    if(ImGui::ColorEdit4("Clear Color", clearColor))
    {
        prefs.clearColorR = clearColor[0];
        prefs.clearColorG = clearColor[1];
        prefs.clearColorB = clearColor[2];
        prefs.clearColorA = clearColor[3];
        persist();
    }
    RestartHint();

    ImGui::SeparatorText("Lighting");

    float drawDistance = mRenderer->GetDrawDistance();
    if(ImGui::DragFloat("Draw Distance", &drawDistance, 1.0f, 1.0f, 100000.0f, "%.1f"))
    {
        mRenderer->SetDrawDistance(drawDistance);
        prefs.drawDistance = drawDistance;
        persist();
    }

    float ibl = mLightService->GetIblIntensity();
    if(ImGui::SliderFloat("IBL Intensity", &ibl, 0.0f, 5.0f, "%.2f"))
    {
        mLightService->SetIblIntensity(ibl);
        prefs.iblIntensity = ibl;
        persist();
    }

    float exposure = mLightService->GetExposure();
    if(ImGui::SliderFloat("Exposure", &exposure, 0.0f, 5.0f, "%.2f"))
    {
        mLightService->SetExposure(exposure);
        prefs.exposure = exposure;
        persist();
    }

    float ambientColor[3] = {
        static_cast<float>(prefs.ambientColorR),
        static_cast<float>(prefs.ambientColorG),
        static_cast<float>(prefs.ambientColorB),
    };
    float ambientIntensity = static_cast<float>(prefs.ambientIntensity);
    bool  ambientChanged   = ImGui::ColorEdit3("Ambient Color", ambientColor);
    ambientChanged =
        ImGui::SliderFloat("Ambient Intensity", &ambientIntensity, 0.0f, 1.0f, "%.3f") ||
        ambientChanged;
    if(ambientChanged)
    {
        const glm::vec3 color(ambientColor[0], ambientColor[1], ambientColor[2]);
        mRenderer->SetAmbient(color, ambientIntensity);
        mFreyaOptions->ambientColor     = color;
        mFreyaOptions->ambientIntensity = ambientIntensity;
        prefs.ambientColorR             = ambientColor[0];
        prefs.ambientColorG             = ambientColor[1];
        prefs.ambientColorB             = ambientColor[2];
        prefs.ambientIntensity          = ambientIntensity;
        persist();
    }

    int maxLights = static_cast<int>(prefs.maxLights);
    if(ImGui::InputInt("Max Lights", &maxLights))
    {
        prefs.maxLights = static_cast<std::uint32_t>(std::max(1, maxLights));
        persist();
    }
    RestartHint();
    ActiveU32("maxLights", mFreyaOptions->maxLights);

    ImGui::SeparatorText("Post-processing");

    if(ImGui::Checkbox("SSAO", &prefs.enableSsao))
    {
        persist();
    }
    RestartHint();
    ActiveBool("SSAO", mFreyaOptions->enableSsao);

    if(ImGui::Checkbox("TAA", &prefs.enableTaa))
    {
        persist();
    }
    RestartHint();
    ActiveBool("TAA", mFreyaOptions->enableTaa);

    if(ImGui::Checkbox("Bloom", &prefs.enableBloom))
    {
        persist();
    }
    RestartHint();
    ActiveBool("Bloom", mFreyaOptions->enableBloom);

    ImGui::SeparatorText("Shadows");

    int shadowQuality = mPendingGraphics.shadowQuality.has_value()
                            ? static_cast<int>(*mPendingGraphics.shadowQuality)
                            : static_cast<int>(mRenderer->GetShadowQuality());
    if(ImGui::Combo("Quality", &shadowQuality, "Low\0Medium\0High\0Ultra\0"))
    {
        const auto quality =
            static_cast<fra::ShadowQuality>(std::clamp(shadowQuality, 0, 3));
        mPendingGraphics.shadowQuality = quality;
        prefs.shadowQuality            = shadowQuality;
        persist();
    }

    int cascades = static_cast<int>(prefs.shadowCascadeCount);
    if(ImGui::SliderInt("Cascade Count", &cascades, 1, 4))
    {
        prefs.shadowCascadeCount = static_cast<std::uint32_t>(cascades);
        persist();
    }
    RestartHint();
    ActiveU32("cascades", mFreyaOptions->shadowCascadeCount);

    int resolution = static_cast<int>(prefs.shadowMapResolution);
    if(ImGui::InputInt("Map Resolution", &resolution))
    {
        prefs.shadowMapResolution =
            static_cast<std::uint32_t>(std::max(64, resolution));
        persist();
    }
    RestartHint();
    ActiveU32("resolution", mFreyaOptions->shadowMapResolution);

    float bias = static_cast<float>(prefs.shadowBias);
    if(ImGui::DragFloat("Bias", &bias, 0.0001f, 0.0f, 0.1f, "%.5f"))
    {
        prefs.shadowBias = bias;
        persist();
    }
    RestartHint();
    ActiveFloat("bias", mFreyaOptions->shadowBias);

    float lightSize = static_cast<float>(prefs.shadowLightSize);
    if(ImGui::DragFloat("Light Size", &lightSize, 0.001f, 0.0f, 1.0f, "%.4f"))
    {
        prefs.shadowLightSize = lightSize;
        persist();
    }
    RestartHint();
    ActiveFloat("lightSize", mFreyaOptions->shadowLightSize);

    float maxSoftness = static_cast<float>(prefs.shadowMaxSoftness);
    if(ImGui::DragFloat("Max Softness", &maxSoftness, 0.1f, 0.0f, 64.0f, "%.2f"))
    {
        prefs.shadowMaxSoftness = maxSoftness;
        persist();
    }
    RestartHint();
    ActiveFloat("maxSoftness", mFreyaOptions->shadowMaxSoftness);

    float minVisibility = static_cast<float>(prefs.shadowMinVisibility);
    if(ImGui::SliderFloat("Min Visibility", &minVisibility, 0.0f, 1.0f, "%.3f"))
    {
        prefs.shadowMinVisibility = minVisibility;
        persist();
    }
    RestartHint();
    ActiveFloat("minVisibility", mFreyaOptions->shadowMinVisibility);

    int maxSpot = static_cast<int>(prefs.maxSpotShadows);
    if(ImGui::SliderInt("Max Spot Shadows", &maxSpot, 0, 4))
    {
        prefs.maxSpotShadows = static_cast<std::uint32_t>(maxSpot);
        persist();
    }
    RestartHint();
    ActiveU32("maxSpotShadows", mFreyaOptions->maxSpotShadows);

    int maxPoint = static_cast<int>(prefs.maxPointShadows);
    if(ImGui::SliderInt("Max Point Shadows", &maxPoint, 0, 2))
    {
        prefs.maxPointShadows = static_cast<std::uint32_t>(maxPoint);
        persist();
    }
    RestartHint();
    ActiveU32("maxPointShadows", mFreyaOptions->maxPointShadows);

    int sampleCount = static_cast<int>(prefs.shadowSampleCount);
    if(ImGui::SliderInt("Sample Count", &sampleCount, 1, 16))
    {
        prefs.shadowSampleCount = static_cast<std::uint32_t>(sampleCount);
        persist();
    }
    RestartHint();
    ActiveU32("shadowSamples", mFreyaOptions->shadowSampleCount);

    ImGui::SeparatorText("Paths");

    char environmentPath[512] {};
    std::strncpy(environmentPath, prefs.environmentMapPath.c_str(),
                 sizeof(environmentPath) - 1);
    if(ImGui::InputText("Environment Map", environmentPath, sizeof(environmentPath)))
    {
        prefs.environmentMapPath = environmentPath;
        persist();
    }
    RestartHint();
    ActivePath("environment", mFreyaOptions->environmentMapPath);

    char shaderRoot[512] {};
    std::strncpy(shaderRoot, prefs.shaderRoot.c_str(), sizeof(shaderRoot) - 1);
    if(ImGui::InputText("Shader Root", shaderRoot, sizeof(shaderRoot)))
    {
        prefs.shaderRoot = shaderRoot;
        persist();
    }
    RestartHint();
    ActivePath("shaderRoot", mFreyaOptions->shaderRoot);

    ImGui::SeparatorText("Depth");

    if(ImGui::Checkbox("Reverse Z", &prefs.reverseZ))
    {
        persist();
    }
    RestartHint();
    ActiveBool("Reverse Z", mFreyaOptions->ReverseZ);
}

void PreferencesLayer::drawAppearanceTab()
{
    auto &prefs = *mPreferences;

    if(ImGui::Combo("Theme", &prefs.appearance.themeIndex,
                    "PhantomDark\0PhantomLight\0Dark\0Light\0Classic\0"))
    {
        applyTheme(prefs.appearance.themeIndex);
        persist();
    }
}

void PreferencesLayer::drawEcsTab()
{
    auto &prefs = mPreferences->ecs;

    ImGui::TextWrapped(
        "Freyr ECS options are applied when the editor starts. "
        "Edits are saved to preferences.json.");

    ImGui::SeparatorText("Preferred (next launch)");

    int maxEntities = static_cast<int>(prefs.maxEntities);
    if(ImGui::InputInt("Max Entities", &maxEntities))
    {
        prefs.maxEntities = static_cast<std::uint64_t>(std::max(1, maxEntities));
        persist();
    }
    RestartHint();

    int chunkCapacity = static_cast<int>(prefs.archetypeChunkCapacity);
    if(ImGui::InputInt("Archetype Chunk Capacity", &chunkCapacity))
    {
        prefs.archetypeChunkCapacity =
            static_cast<std::uint64_t>(std::max(1, chunkCapacity));
        persist();
    }
    RestartHint();

    int threadCount = static_cast<int>(prefs.threadCount);
    if(ImGui::InputInt("Thread Count", &threadCount))
    {
        prefs.threadCount = static_cast<std::uint64_t>(std::max(1, threadCount));
        persist();
    }
    RestartHint();

    ImGui::SeparatorText("Active");
    ImGui::Text("Max Entities: %llu",
                static_cast<unsigned long long>(mFreyrOptions->MaxEntities));
    ImGui::Text("Chunk Capacity: %llu",
                static_cast<unsigned long long>(mFreyrOptions->ArchetypeChunkCapacity));
    ImGui::Text("Thread Count: %llu",
                static_cast<unsigned long long>(mFreyrOptions->ThreadCount));

    const bool pendingRestart =
        prefs.maxEntities != mFreyrOptions->MaxEntities ||
        prefs.archetypeChunkCapacity != mFreyrOptions->ArchetypeChunkCapacity ||
        prefs.threadCount != mFreyrOptions->ThreadCount;

    if(pendingRestart)
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f),
                           "Restart required to apply ECS changes.");
    }
}

void PreferencesLayer::onGui()
{
    if(!IsOpen)
    {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(520, 640), ImGuiCond_FirstUseEver);
    if(ImGui::Begin("Preferences", &IsOpen, ImGuiWindowFlags_NoCollapse))
    {
        if(ImGui::BeginTabBar("PreferencesTabs"))
        {
            if(ImGui::BeginTabItem("Appearance"))
            {
                drawAppearanceTab();
                ImGui::EndTabItem();
            }
            if(ImGui::BeginTabItem("Graphics"))
            {
                drawGraphicsTab();
                ImGui::EndTabItem();
            }
            if(ImGui::BeginTabItem("ECS"))
            {
                drawEcsTab();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}
