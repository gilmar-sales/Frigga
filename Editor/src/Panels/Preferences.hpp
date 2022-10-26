#pragma once

#include <Core/Layer.hpp>


class PreferencesLayer: public fg::Layer
{
  public:
    PreferencesLayer(): fg::Layer("Preferences") {}
    ~PreferencesLayer() = default;

    void onGui() override;

    static bool IsOpen;

  private:
};