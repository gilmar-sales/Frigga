#include "systems/CharacterMovementSystem.hpp"
#include "components/CharacterControllerComponent.hpp"

#include <Frigga/Module/FriModule.hpp>

#include <cstdio>

static void DrawCharacterController(CharacterControllerComponent &c, fg::FriComponentInspector &ui)
{
    ui.BeginDisabled(ui.playing);
    ui.DragFloat("Max Slope", c.maxSlopeDegrees, 0.5f, 1.0f, 89.0f);
    ui.DragFloat("Max Strength", c.maxStrength, 1.0f, 0.0f, 10000.0f);
    if(ui.IsItemHovered())
    {
        ui.SetTooltip("Max force (N) when pushing dynamic bodies.\n"
                      "Capsule shape, mass, and layers live on RigidBody.");
    }
    ui.DragFloat("Stick To Floor", c.stickToFloorDistance, 0.01f, 0.0f, 2.0f);
    if(ui.IsItemHovered())
    {
        ui.SetTooltip("Downward snap distance while grounded. 0 disables.");
    }
    ui.DragFloat("Walk Stairs Height", c.walkStairsStepHeight, 0.01f, 0.0f, 2.0f);
    if(ui.IsItemHovered())
    {
        ui.SetTooltip("Max step-up height. 0 disables stair walking.");
    }
    ui.Checkbox("Locomotion Locked", c.locomotionLocked);
    ui.EndDisabled();

    if(ui.hasCharacter)
    {
        char id[64];
        std::snprintf(id, sizeof(id), "Character ID: %u", ui.characterId);
        ui.TextDisabled(id);
    }
    else if(ui.playing)
    {
        ui.TextDisabled("Controller edits apply after Stop");
    }
    ui.TextDisabled("Requires RigidBody (capsule presence)");
}

FRI_MODULE(module)
{
    module.Component<CharacterControllerComponent>("CharacterControllerComponent",
                                                   "Character Controller", DrawCharacterController)
          .System<CharacterMovementSystem>();
}
