#include "systems/CharacterMovementSystem.hpp"
#include "components/CharacterControllerComponent.hpp"

#include <Frigga/Plugin/FriPluginModule.hpp>

#include <cstdint>
#include <cstdio>

static void DrawCharacterController(CharacterControllerComponent &c, fg::FriComponentInspector &ui)
{
    ui.BeginDisabled(ui.playing);
    ui.DragFloat("Radius", c.radius, 0.01f, 0.05f, 5.0f);
    ui.DragFloat("Height", c.height, 0.01f, 0.05f, 5.0f);
    ui.DragFloat3("Center Offset", c.centerOffset, 0.01f);
    if(ui.IsItemHovered())
    {
        ui.SetTooltip("Local offset of the capsule center from Transform.\n"
                      "Built-in feet lift (0, height/2+radius, 0) is always applied first.");
    }
    ui.DragFloat("Max Slope", c.maxSlopeDegrees, 0.5f, 1.0f, 89.0f);
    ui.DragFloat("Mass", c.mass, 0.5f, 1.0f, 500.0f);
    int layer = static_cast<int>(c.collisionLayer);
    if(ui.SliderInt("Collision Layer", layer, 0, 15))
    {
        c.collisionLayer = static_cast<std::uint8_t>(layer);
    }
    char mask[32];
    std::snprintf(mask, sizeof(mask), "Mask: 0x%04X", c.collideWithLayers);
    ui.TextDisabled(mask);
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
}

FRI_PLUGIN_MODULE(plugin)
{
    plugin.Component<CharacterControllerComponent>("CharacterControllerComponent",
                                                   "Character Controller", DrawCharacterController)
          .System<CharacterMovementSystem>();
}
