#pragma once

#include <Frigga/Frigga.hpp>

class PreferencesLayer: public fg::Layer
{
  public:
    PreferencesLayer(const Ref<fra::Window> &window, const Ref<fra::Renderer> &renderer)
        : fg::Layer("Preferences"), mWindow(window), mRenderer(renderer)
    {
    }
    ~PreferencesLayer() = default;

    void onGui() override;

    static bool IsOpen;

  private:
    Ref<fra::Window> mWindow;
    Ref<fra::Renderer> mRenderer;
};