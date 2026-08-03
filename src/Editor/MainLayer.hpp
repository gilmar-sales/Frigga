#pragma once

#include <Frigga/Core/LayerStack.hpp>
#include <Frigga/Scene/Scene.hpp>

class HierarchyLayer;
class Workflow;
class MainLayer: public fg::Layer
{
  public:
    MainLayer(skr::Arc<fg::Scene> scene, skr::Arc<fg::LayerStack> layerStack, skr::Arc<fra::Window> window,
              skr::Arc<skr::ServiceProvider> serviceProvider);
    ~MainLayer() = default;

    void onUpdate() override;
    void onGui() override;

    float drawTitleBar();
    void drawMenuBar();

  private:
    std::vector<std::pair<const char *, skr::Arc<Workflow>>> m_tabIds;
    skr::Arc<Workflow> m_activeTab;
    const char *m_activeTabName = "Gameplay";
    bool m_resetDockLayout      = false;

    skr::Arc<fg::Scene> mScene;
    skr::Arc<fg::LayerStack> mLayerStack;
    skr::Arc<fra::Window> mWindow;
    skr::Arc<HierarchyLayer> mHierarchy;
};
