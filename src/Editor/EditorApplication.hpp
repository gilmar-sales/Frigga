#pragma once

#include "Frigga/Frigga.hpp"
#include "MainLayer.hpp"

class EditorApplication final: public fg::AbstractApplication
{
  public:
    EditorApplication(const Ref<skr::ServiceProvider> &serviceProvider)
        : AbstractApplication(serviceProvider)
    {
        PushLayer(serviceProvider->GetService<MainLayer>());
    }
};
