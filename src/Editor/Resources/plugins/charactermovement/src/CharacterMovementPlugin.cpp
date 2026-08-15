#include "systems/CharacterMovementSystem.hpp"
#include "components/CharacterControllerComponent.hpp"

#include <Frigga/Plugin/FriPluginModule.hpp>

FRI_PLUGIN_MODULE(plugin)
{
    plugin.Component<CharacterControllerComponent>("CharacterControllerComponent",
                                                   "Character Controller")
          .System<CharacterMovementSystem>();
}
