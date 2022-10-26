#pragma once

#include <Core/Layer.hpp>
#include <Core/LayerStack.hpp>

class Workflow: public fg::Layer
{
  public:
    Workflow(std::string name): fg::Layer(name) {}
    Workflow(std::string name, std::vector<fg::Layer *> layers = {});

    ~Workflow() = default;

    void onGui() override;
    void onUpdate() override;
    void onEvent(fg::Event &event) override;

  private:
    fg::LayerStack m_layerStack;
};
