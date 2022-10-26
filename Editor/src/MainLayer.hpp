#pragma once

#include <Core/LayerStack.hpp>
#include <Platform/OpenGL/OpenGLFrameBuffer.hpp>
#include <Scene/Scene.hpp>

#include "Workflows/Workflow.hpp"

class MainLayer: public fg::Layer
{
  public:
    MainLayer(shared<fg::Scene> scene);
    ~MainLayer() = default;

    virtual void onGui() override;

    float drawTitleBar();
    void drawMenuBar();

  private:
    std::vector<std::pair<const char *, Workflow *>> m_tabIds;
    Workflow *m_activeTab;

    shared<fg::Scene> m_scene;
    fg::LayerStack m_layerStack;
};
