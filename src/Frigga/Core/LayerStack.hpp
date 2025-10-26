#pragma once

#include "Layer.hpp"

namespace FRIGGA_NAMESPACE
{

    class LayerStack
    {
      private:
        std::vector<Layer *> m_layers;
        unsigned m_lastIndex;

      public:
        LayerStack(): m_lastIndex(0) {};
        ~LayerStack();

        void pushLayer(Layer *layer);
        void pushOverlay(Layer *layer);
        void popLayer(Layer *layer);
        void popOverlay(Layer *layer);

        auto begin()
        {
            return m_layers.begin();
        }

        auto end()
        {
            return m_layers.end();
        }

        auto rbegin()
        {
            return m_layers.rbegin();
        }

        auto rend()
        {
            return m_layers.rend();
        }

        auto begin() const
        {
            return m_layers.begin();
        }

        auto end() const
        {
            return m_layers.end();
        }

        auto rbegin() const
        {
            return m_layers.rbegin();
        }

        auto rend() const
        {
            return m_layers.rend();
        }
    };

} // namespace FRIGGA_NAMESPACE
