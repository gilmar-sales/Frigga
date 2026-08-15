#include "systems/ThirdPersonCameraSystem.hpp"
#include "components/ThirdPersonCameraComponent.hpp"

#include <Frigga/Plugin/FriPluginModule.hpp>

FRI_PLUGIN_MODULE(plugin)
{
    plugin.Component<ThirdPersonCameraComponent>("ThirdPersonCameraComponent",
                                                 "Third Person Camera")
          .System<ThirdPersonCameraSystem>("Main");
}
