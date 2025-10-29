#pragma once

#include <Frigga/Core/Layer.hpp>
#include <Frigga/Core/LayerStack.hpp>

class Workflow: public fg::Layer
{
  public:
    Workflow(std::string name): fg::Layer(name) {}
    Workflow(std::string name, std::vector<Ref<fg::Layer>> layers = {});

    ~Workflow() = default;

    void onGui() override;
    void onUpdate() override;
    void onEvent(fg::Event &event) override;

  private:
    fg::LayerStack m_layerStack;
};