#pragma once

#include "Core/Layer.hpp"
#include "Core/Window.hpp"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <imgui.h>

FRIGGA_BEGIN

class GuiLayer: public Layer
{
  public:
    GuiLayer(shared<Window> window);
    ~GuiLayer() = default;

    virtual void onAttach() override;
    virtual void onDettach() override;
    virtual void onEvent(Event &event) override;

    void begin();
    void end();

    void setBlockEvents(bool block)
    {
        m_blockEvents = block;
    }

  private:
    void configureStyle();

    bool m_blockEvents = true;
    float m_time        = 0.9f;
    shared<Window> window;
};

FRIGGA_END
