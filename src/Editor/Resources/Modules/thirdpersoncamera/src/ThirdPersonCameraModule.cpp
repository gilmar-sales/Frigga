#include "systems/ThirdPersonCameraSystem.hpp"
#include "components/ThirdPersonCameraComponent.hpp"

#include <Frigga/Module/FriModule.hpp>

static void DrawThirdPersonCamera(ThirdPersonCameraComponent &c, fg::FriComponentInspector &ui)
{
    ui.InputText("Target", c.targetName);
    ui.DragFloat3("Pivot Offset", c.pivotOffset, 0.01f);
    ui.DragFloat("Distance", c.distance, 0.05f, c.minDistance, c.maxDistance);
    ui.DragFloat("Min Distance", c.minDistance, 0.05f, 0.1f, 50.0f);
    ui.DragFloat("Max Distance", c.maxDistance, 0.05f, 0.1f, 50.0f);
    ui.DragFloat("Yaw", c.yaw, 0.5f);
    ui.DragFloat("Pitch", c.pitch, 0.5f, c.minPitch, c.maxPitch);
    ui.DragFloat("Min Pitch", c.minPitch, 0.5f, -89.0f, 89.0f);
    ui.DragFloat("Max Pitch", c.maxPitch, 0.5f, -89.0f, 89.0f);
    ui.InputText("Look X Axis", c.lookXAxis);
    ui.InputText("Look Y Axis", c.lookYAxis);
    ui.InputText("Zoom Axis", c.zoomAxis);
    ui.TextDisabled("Uses Input Map LookX / LookY / Zoom");
}

FRI_MODULE(module)
{
    module.Component<ThirdPersonCameraComponent>("ThirdPersonCameraComponent",
                                                 "Third Person Camera", DrawThirdPersonCamera)
          .System<ThirdPersonCameraSystem>("Main");
}
