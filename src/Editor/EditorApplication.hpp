#pragma once

#include <Frigga/Frigga.hpp>

#include "MainLayer.hpp"
#include "Panels/HierarchyLayer.hpp"
#include "Panels/PreferencesLayer.hpp"
#include "Panels/ResourcesLayer.hpp"

class EditorApplication final: public fg::AbstractApplication
{
  public:
    EditorApplication(const Ref<skr::ServiceProvider> &serviceProvider)
        : AbstractApplication(serviceProvider)
    {
        PushLayer(mScope->GetServiceProvider()->GetService<MainLayer>());
        PushLayer(mScope->GetServiceProvider()->GetService<HierarchyLayer>());
        PushLayer(mScope->GetServiceProvider()->GetService<PreferencesLayer>());
        PushLayer(mScope->GetServiceProvider()->GetService<ResourcesLayer>());
    }

  private:
};
