#pragma once

#include <Frigga/Frigga.hpp>

class PreferencesLayer: public fg::Layer
{
  public:
    PreferencesLayer(const skr::Arc<fra::Window> &window, const skr::Arc<fra::Renderer> &renderer)
        : fg::Layer("Preferences"), mWindow(window), mRenderer(renderer)
    {
    }
    ~PreferencesLayer() = default;

    void onGui() override;

    static bool IsOpen;

  private:
    skr::Arc<fra::Window> mWindow;
    skr::Arc<fra::Renderer> mRenderer;
};