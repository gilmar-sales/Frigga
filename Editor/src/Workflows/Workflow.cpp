#include "Workflow.hpp"

Workflow::Workflow(std::string name, std::vector<fg::Layer *> layers): fg::Layer(name)
{
    for(auto layer: layers)
    {
        m_layerStack.pushLayer(layer);
    }
}

void Workflow::onGui()
{
    for(auto layer: m_layerStack)
    {
        layer->onGui();
    }
}

void Workflow::onUpdate()
{
    for(auto layer: m_layerStack)
    {
        layer->onUpdate();
    }
}

void Workflow::onEvent(fg::Event &event)
{
    for(auto layer: m_layerStack)
    {
        layer->onEvent(event);
    }
}