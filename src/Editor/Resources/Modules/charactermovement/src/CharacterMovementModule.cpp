#include "components/CharacterControllerComponent.hpp"
#include "systems/CharacterMovementSystem.hpp"

#include <Frigga/Module/FriModule.hpp>

#include <cstdio>

static void DrawCharacterController(CharacterControllerComponent &c, fg::FriComponentInspector &ui)
{
    ui.BeginDisabled(ui.playing);
    ui.DragFloat("Max Slope", c.maxSlopeDegrees, 0.5f, 1.0f, 89.0f);
    if(ui.IsItemHovered())
    {
        ui.SetTooltip("Max walkable ground slope (degrees).\n"
                      "Shape, mass, and layers live on Dynamic RigidBody.");
    }
    ui.DragFloat("Movement Speed", c.movementSpeed, 0.1f, 0.0f, 100.0f);
    ui.DragFloat("Jump Speed", c.jumpSpeed, 0.1f, 0.0f, 100.0f);
    ui.Checkbox("Locomotion Locked", c.locomotionLocked);
    ui.EndDisabled();

    if(ui.hasCharacter)
    {
        char id[64];
        std::snprintf(id, sizeof(id), "Body ID: %u", ui.characterId);
        ui.TextDisabled(id);
    }
    else if(ui.playing)
    {
        ui.TextDisabled("Controller edits apply after Stop");
    }
    ui.TextDisabled("Requires Dynamic RigidBody");
}

FRI_MODULE(module)
{
    module
        .Component<CharacterControllerComponent>("CharacterControllerComponent",
                                                 "Character Controller", DrawCharacterController)
        .System<CharacterMovementSystem>();
}
