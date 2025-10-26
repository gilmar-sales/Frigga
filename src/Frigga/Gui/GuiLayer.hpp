#pragma once

#include "Freya/Freya.hpp"

#include "Frigga/Core/Layer.hpp"

namespace FRIGGA_NAMESPACE
{

    class GuiLayer: public Layer
    {
      public:
        GuiLayer(Ref<fra::Window> window);
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
        float m_time       = 0.9f;
        Ref<fra::Window> mWindow;
    };

} // namespace FRIGGA_NAMESPACE
