#pragma once

#include "../Preferences/EditorPreferences.hpp"

#include <Freya/Core/LightService.hpp>
#include <Freya/FreyaOptions.hpp>
#include <Frigga/Frigga.hpp>

#include <optional>

class PreferencesLayer: public fg::Layer
{
  public:
    PreferencesLayer(const skr::Arc<skr::ServiceProvider> &services,
                     const skr::Arc<fra::Window> &window,
                     const skr::Arc<fra::Renderer> &renderer,
                     const skr::Arc<fra::LightService> &lightService,
                     const skr::Arc<fra::FreyaOptions> &freyaOptions,
                     const skr::Arc<fr::FreyrOptions> &freyrOptions,
                     const skr::Arc<EditorPreferences> &preferences)
        : fg::Layer("Preferences"),
          mServices(services),
          mWindow(window),
          mRenderer(renderer),
          mLightService(lightService),
          mFreyaOptions(freyaOptions),
          mFreyrOptions(freyrOptions),
          mPreferences(preferences)
    {
    }

    ~PreferencesLayer() = default;

    void onUpdate() override;
    void onGui() override;

    static bool IsOpen;

  private:
    /// Renderer mutations that rebuild GPU resources. Must run in onUpdate
    /// (before BeginFrame), never mid-ImGui while the UI pass is open.
    struct PendingGraphics
    {
        std::optional<bool>        vSync;
        std::optional<std::string> environmentMapPath;
        std::optional<std::string> shaderRoot;
        std::optional<bool>        reverseZ;
    };

    void drawAppearanceTab();
    void drawGraphicsTab();
    void drawEcsTab();
    void drawViewportQuality(const char *label, ViewportQualityPreferences &quality);
    void persist();
    void applyTheme(int themeIndex) const;
    void applyPendingGraphics();
    void syncSsaoFinePrefsFromRenderer();

    skr::Arc<skr::ServiceProvider> mServices;
    skr::Arc<fra::Window>           mWindow;
    skr::Arc<fra::Renderer>         mRenderer;
    skr::Arc<fra::LightService>     mLightService;
    skr::Arc<fra::FreyaOptions>     mFreyaOptions;
    skr::Arc<fr::FreyrOptions>      mFreyrOptions;
    skr::Arc<EditorPreferences>     mPreferences;
    PendingGraphics                 mPendingGraphics {};
};
