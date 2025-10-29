#pragma once

#include <Frigga/Frigga.hpp>

class PreferencesLayer: public fg::Layer
{
  public:
    PreferencesLayer(const Ref<fra::Window> &window): fg::Layer("Preferences"), mWindow(window) {}
    ~PreferencesLayer() = default;

    void onGui() override;

    static bool IsOpen;

  private:
    Ref<fra::Window> mWindow;
};