#include "PreferencesLayer.hpp"

#include "../Preferences/PreferencesStore.hpp"

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
} // namespace

void PreferencesLayer::persist()
{
    PreferencesStore::Save(*mPreferences);
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

void PreferencesLayer::drawGraphicsTab()
{
    auto &prefs = mPreferences->graphics;

    ImGui::SeparatorText("Display");

    bool vSync = mRenderer->GetVSync();
    if(ImGui::Checkbox("VSync", &vSync))
    {
        mRenderer->SetVSync(vSync);
        prefs.vSync = vSync;
        persist();
    }

    bool fullscreen = mWindow->IsFullscreen();
    if(ImGui::Checkbox("Fullscreen", &fullscreen))
    {
        mWindow->SetFullscreen(fullscreen);
        prefs.fullscreen = fullscreen;
        persist();
    }

    int sampleIndex = SampleCountToIndex(mRenderer->GetSamples());
    if(ImGui::Combo("MSAA", &sampleIndex, "1x\02x\04x\08x\0"))
    {
        const auto samples = IndexToSampleCount(sampleIndex);
        mRenderer->SetSamples(samples);
        prefs.sampleCount = samples;
        persist();
    }

    ImGui::SeparatorText("Rendering");

    float drawDistance = mRenderer->GetDrawDistance();
    if(ImGui::DragFloat("Draw Distance", &drawDistance, 1.0f, 1.0f, 100000.0f, "%.1f"))
    {
        mRenderer->SetDrawDistance(drawDistance);
        prefs.drawDistance = drawDistance;
        persist();
    }

    int strategy = static_cast<int>(mRenderer->GetRenderingStrategy());
    if(ImGui::Combo("Strategy", &strategy, "Forward\0Deferred\0"))
    {
        mRenderer->SetRenderingStrategy(static_cast<fra::RenderingStrategy>(strategy));
        prefs.renderingStrategy = strategy;
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

    ImGui::SeparatorText("Advanced (next launch)");

    int frameCount = static_cast<int>(prefs.frameCount);
    if(ImGui::InputInt("Frame Count", &frameCount))
    {
        prefs.frameCount = static_cast<std::uint32_t>(std::max(1, frameCount));
        persist();
    }
    RestartHint();
    ImGui::TextDisabled("Active: %u", mFreyaOptions->frameCount);

    int maxLights = static_cast<int>(prefs.maxLights);
    if(ImGui::InputInt("Max Lights", &maxLights))
    {
        prefs.maxLights = static_cast<std::uint32_t>(std::max(1, maxLights));
        persist();
    }
    RestartHint();
    ImGui::TextDisabled("Active: %u", mFreyaOptions->maxLights);

    char environmentPath[512] {};
    std::strncpy(environmentPath, prefs.environmentMapPath.c_str(), sizeof(environmentPath) - 1);
    if(ImGui::InputText("Environment Map", environmentPath, sizeof(environmentPath)))
    {
        prefs.environmentMapPath = environmentPath;
        persist();
    }
    RestartHint();

    if(ImGui::Checkbox("Reverse Z", &prefs.reverseZ))
    {
        persist();
    }
    RestartHint();
    ImGui::TextDisabled("Active: %s", mFreyaOptions->ReverseZ ? "on" : "off");
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

    ImGui::SetNextWindowSize(ImVec2(480, 0), ImGuiCond_FirstUseEver);
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
