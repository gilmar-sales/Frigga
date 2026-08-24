#include "PreferencesLayer.hpp"

#include "../EditorTheme.hpp"
#include "../Preferences/PreferencesStore.hpp"
#include "../UiScale.hpp"

#include <Frigga/Gui/GuiLayer.hpp>
#include <imgui.h>

#include <algorithm>
#include <cstring>

bool PreferencesLayer::IsOpen = false;

namespace
{
    constexpr const char *kQualityLabels = "Low\0Medium\0High\0Ultra\0Off\0";

    int ClampQuality(int value)
    {
        return std::clamp(value, 0, 4);
    }

    void RestartHint()
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(restart)");
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Saved to the OS preferred config dir. Requires editor restart.");
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

    void ActivePath(const char *label, const std::string &value)
    {
        ImGui::TextDisabled("Active %s: %s", label, value.c_str());
    }
} // namespace

void PreferencesLayer::persist()
{
    PreferencesStore::Save(*mPreferences);
}

void PreferencesLayer::syncSsaoFinePrefsFromRenderer()
{
    auto &prefs          = mPreferences->graphics;
    prefs.ssaoRadius     = mRenderer->GetSsaoRadius();
    prefs.ssaoBias       = mRenderer->GetSsaoBias();
    prefs.ssaoPower      = mRenderer->GetSsaoPower();
    prefs.ssaoIntensity  = mRenderer->GetSsaoIntensity();
    prefs.ssaoDebugView  = static_cast<int>(mRenderer->GetSsaoDebugView());
}

void PreferencesLayer::applyTheme(int themeIndex) const
{
    EditorTheme::Apply(themeIndex);
}

void PreferencesLayer::applyPendingGraphics()
{
    if(!mPendingGraphics.vSync.has_value())
    {
        return;
    }

    mRenderer->SetVSync(*mPendingGraphics.vSync);
    mPendingGraphics.vSync.reset();
    fg::GuiLayer::RecreateMainPipeline(mRenderer);
}

void PreferencesLayer::drawViewportQuality(const char *label, ViewportQualityPreferences &quality)
{
    ImGui::PushID(label);
    ImGui::SeparatorText(label);

    int shadowQuality = ClampQuality(quality.shadowQuality);
    if(ImGui::Combo("Shadows", &shadowQuality, kQualityLabels))
    {
        quality.shadowQuality = ClampQuality(shadowQuality);
        persist();
    }

    int ssaoQuality = ClampQuality(quality.ssaoQuality);
    if(ImGui::Combo("SSAO", &ssaoQuality, kQualityLabels))
    {
        quality.ssaoQuality = ClampQuality(ssaoQuality);
        persist();
    }

    int taaQuality = ClampQuality(quality.taaQuality);
    if(ImGui::Combo("TAA", &taaQuality, kQualityLabels))
    {
        quality.taaQuality = ClampQuality(taaQuality);
        persist();
    }

    int bloomQuality = ClampQuality(quality.bloomQuality);
    if(ImGui::Combo("Bloom", &bloomQuality, kQualityLabels))
    {
        quality.bloomQuality = ClampQuality(bloomQuality);
        persist();
    }

    ImGui::PopID();
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

    drawViewportQuality("Editor Viewport", prefs.editorViewport);
    ImGui::TextDisabled("Also used by Animation Preview.");

    drawViewportQuality("Gameplay Viewport", prefs.gameplayViewport);
    ImGui::TextDisabled("Applied while the Gameplay viewport is rendering.");

    ImGui::SeparatorText("SSAO Fine Tuning");
    ImGui::TextDisabled("Shared knobs applied on top of the active viewport preset.");

    float radius = mRenderer->GetSsaoRadius();
    if(ImGui::DragFloat("SSAO Radius", &radius, 0.01f, 0.05f, 4.0f, "%.3f"))
    {
        mRenderer->SetSsaoRadius(radius);
        prefs.ssaoRadius = radius;
        persist();
    }

    float bias = mRenderer->GetSsaoBias();
    if(ImGui::DragFloat("SSAO Bias", &bias, 0.001f, 0.0f, 0.2f, "%.4f"))
    {
        mRenderer->SetSsaoBias(bias);
        prefs.ssaoBias = bias;
        persist();
    }

    float power = mRenderer->GetSsaoPower();
    if(ImGui::DragFloat("SSAO Power", &power, 0.01f, 0.1f, 4.0f, "%.2f"))
    {
        mRenderer->SetSsaoPower(power);
        prefs.ssaoPower = power;
        persist();
    }

    float intensity = mRenderer->GetSsaoIntensity();
    if(ImGui::SliderFloat("SSAO Intensity", &intensity, 0.0f, 2.0f, "%.2f"))
    {
        mRenderer->SetSsaoIntensity(intensity);
        prefs.ssaoIntensity = intensity;
        persist();
    }

    int debugView = static_cast<int>(mRenderer->GetSsaoDebugView());
    if(ImGui::Combo("SSAO Debug", &debugView, "None\0Blurred\0Raw\0"))
    {
        const auto view = static_cast<fra::SsaoDebugView>(std::clamp(debugView, 0, 2));
        mRenderer->SetSsaoDebugView(view);
        prefs.ssaoDebugView = static_cast<int>(view);
        persist();
    }

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

    ImGui::SeparatorText("Code editor");
    char command[256] {};
    std::strncpy(command, prefs.tools.codeEditorCommand.c_str(), sizeof(command) - 1);
    if(ImGui::InputText("Command", command, sizeof(command)))
    {
        prefs.tools.codeEditorCommand = command;
        persist();
    }
    ImGui::TextDisabled("Launched with the project folder as the first argument.");
    ImGui::TextDisabled("Defaults to VS Code (code). Examples: cursor, codium, code.cmd");
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

    ImGui::SetNextWindowSize(EditorUiScale::V(520.0f, 640.0f), ImGuiCond_FirstUseEver);
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
