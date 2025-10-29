#pragma once

#include <Frigga/Core/LayerStack.hpp>
#include <Frigga/Scene/Scene.hpp>

class Workflow;
class MainLayer: public fg::Layer
{
  public:
    MainLayer(Ref<fg::Scene> scene, Ref<fg::LayerStack> layerStack, Ref<fra::Window> window,
              Ref<skr::ServiceProvider> serviceProvider);
    ~MainLayer() = default;

    virtual void onGui() override;

    float drawTitleBar();
    void drawMenuBar();

  private:
    std::vector<std::pair<const char *, Ref<Workflow>>> m_tabIds;
    Ref<Workflow> m_activeTab;

    Ref<fg::Scene> mScene;
    Ref<fg::LayerStack> mLayerStack;
    Ref<fra::Window> mWindow;
};