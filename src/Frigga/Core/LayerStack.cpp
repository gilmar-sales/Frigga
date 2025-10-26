#include "LayerStack.hpp"

namespace FRIGGA_NAMESPACE
{

    LayerStack::~LayerStack()
    {
        for(Ref<Layer> layer: m_layers)
        {
            layer->onDettach();
        }
    }

    void LayerStack::pushLayer(Ref<Layer> layer)
    {
        m_layers.emplace(m_layers.begin() + m_lastIndex, layer);
        m_lastIndex++;
        layer->onAttach();
    }
    void LayerStack::pushOverlay(Ref<Layer> layer)
    {
        m_layers.emplace_back(layer);
        layer->onAttach();
    }

    void LayerStack::popLayer(Ref<Layer> layer)
    {
        auto it = std::find(m_layers.begin(), m_layers.begin() + m_lastIndex, layer);
        if(it != m_layers.begin() + m_lastIndex)
        {
            layer->onDettach();
            m_layers.erase(it);
            m_lastIndex--;
        }
    }

    void LayerStack::popOverlay(Ref<Layer> layer)
    {
        auto it = std::find(m_layers.begin() + m_lastIndex, m_layers.end(), layer);
        if(it != m_layers.begin() + m_lastIndex)
        {
            layer->onDettach();
            m_layers.erase(it);
            m_lastIndex--;
        }
    }

} // namespace FRIGGA_NAMESPACE