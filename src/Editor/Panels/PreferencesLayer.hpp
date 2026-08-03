#pragma once

#include "../Preferences/EditorPreferences.hpp"

#include <Freya/Core/LightService.hpp>
#include <Freya/FreyaOptions.hpp>
#include <Frigga/Frigga.hpp>

class PreferencesLayer: public fg::Layer
{
  public:
    PreferencesLayer(const skr::Arc<fra::Window> &window,
                     const skr::Arc<fra::Renderer> &renderer,
                     const skr::Arc<fra::LightService> &lightService,
                     const skr::Arc<fra::FreyaOptions> &freyaOptions,
                     const skr::Arc<fr::FreyrOptions> &freyrOptions,
                     const skr::Arc<EditorPreferences> &preferences)
        : fg::Layer("Preferences"),
          mWindow(window),
          mRenderer(renderer),
          mLightService(lightService),
          mFreyaOptions(freyaOptions),
          mFreyrOptions(freyrOptions),
          mPreferences(preferences)
    {
    }

    ~PreferencesLayer() = default;

    void onGui() override;

    static bool IsOpen;

  private:
    void drawAppearanceTab();
    void drawGraphicsTab();
    void drawEcsTab();
    void persist();
    void applyTheme(int themeIndex) const;

    skr::Arc<fra::Window>           mWindow;
    skr::Arc<fra::Renderer>         mRenderer;
    skr::Arc<fra::LightService>     mLightService;
    skr::Arc<fra::FreyaOptions>     mFreyaOptions;
    skr::Arc<fr::FreyrOptions>      mFreyrOptions;
    skr::Arc<EditorPreferences>     mPreferences;
};
