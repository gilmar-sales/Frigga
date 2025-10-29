#pragma once

#include <Frigga/Core/LayerStack.hpp>
#include <Frigga/Scene/Scene.hpp>

class MainLayer: public fg::Layer
{
  public:
    MainLayer(Ref<fg::Scene> scene, Ref<fg::LayerStack> layerStack, Ref<fra::Window> window);
    ~MainLayer() = default;

    virtual void onGui() override;

    float drawTitleBar();
    void drawMenuBar();

  private:
    Ref<fg::Scene> mScene;
    Ref<fg::LayerStack> mLayerStack;
    Ref<fra::Window> mWindow;
};