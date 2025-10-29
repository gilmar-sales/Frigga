#pragma once

#include <vector>

#include "Freya/Core/AbstractApplication.hpp"
#include "Frigga/Core/LayerStack.hpp"
#include "Frigga/Gui/GuiLayer.hpp"
#include "Frigga/Scene/Scene.hpp"

namespace FRIGGA_NAMESPACE
{
#define BIND_EVENT_FN(x) std::bind(&Application::x, this, std::placeholders::_1)

    class AbstractApplication: public fra::AbstractApplication
    {
      public:
        AbstractApplication(const Ref<skr::ServiceProvider> &serviceProvider)
            : fra::AbstractApplication(serviceProvider)
        {
            createScope();
        }

        ~AbstractApplication();

        void Update() override;

        void OnEvent(Event &event);

        void PushLayer(Ref<Layer> layer);
        void PushOverlay(Ref<Layer> layer);

      protected:
        void createScope()
        {
            mScope = mRootServiceProvider->CreateServiceScope();

            PushLayer(mScope->GetServiceProvider()->GetService<GuiLayer>());
        }

        Ref<skr::ServiceScope> mScope;
    };

} // namespace FRIGGA_NAMESPACE