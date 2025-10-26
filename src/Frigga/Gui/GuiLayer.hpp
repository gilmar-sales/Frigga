#pragma once

#include "Freya/Freya.hpp"

#include "Frigga/Core/Layer.hpp"

namespace FRIGGA_NAMESPACE
{

    class GuiLayer: public Layer
    {
      public:
        GuiLayer(Ref<skr::ServiceProvider> serviceProvider)
            : fg::Layer("GuiLayer"), mServiceProvider(serviceProvider)
        {
        }

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
        Ref<skr::ServiceProvider> mServiceProvider;
    };

} // namespace FRIGGA_NAMESPACE
