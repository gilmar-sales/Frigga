#include "LayerStack.hpp"

#include <algorithm>

namespace FRIGGA_NAMESPACE
{

    LayerStack::~LayerStack()
    {
        for(skr::Arc<Layer> layer: m_layers)
        {
            layer->onDettach();
        }
    }

    void LayerStack::pushLayer(skr::Arc<Layer> layer)
    {
        m_layers.emplace(m_layers.begin() + m_lastIndex, layer);
        m_lastIndex++;
        layer->onAttach();
    }
    void LayerStack::pushOverlay(skr::Arc<Layer> layer)
    {
        m_layers.emplace_back(layer);
        layer->onAttach();
    }

    void LayerStack::popLayer(skr::Arc<Layer> layer)
    {
        auto it = std::find(m_layers.begin(), m_layers.begin() + m_lastIndex, layer);
        if(it != m_layers.begin() + m_lastIndex)
        {
            layer->onDettach();
            m_layers.erase(it);
            m_lastIndex--;
        }
    }

    void LayerStack::popOverlay(skr::Arc<Layer> layer)
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