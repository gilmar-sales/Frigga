#include "Frigga/Input/InputMap.hpp"

namespace FRIGGA_NAMESPACE
{

    InputMap MakeDefaultInputMap()
    {
        InputMap map;
        map.version = 1;

        map.actions["Jump"] = InputActionBinding {
            .keys           = {fra::KeyCode::Space},
            .mouseButtons   = {},
            .gamepadButtons = {fra::GamepadButton::GamepadButtonSouth},
        };
        map.actions["Fire"] = InputActionBinding {
            .keys           = {},
            .mouseButtons   = {fra::MouseButton::Left},
            .gamepadButtons = {fra::GamepadButton::GamepadButtonRightShoulder},
        };

        map.axes["Horizontal"] = InputAxisBinding {
            .negativeKeys   = {fra::KeyCode::A, fra::KeyCode::Left},
            .positiveKeys   = {fra::KeyCode::D, fra::KeyCode::Right},
            .gamepadAxis    = fra::GamepadAxis::GamepadAxisLeftX,
            .deadzone       = 0.15f,
            .scale          = 1.0f,
            .invertGamepad  = false,
        };
        map.axes["Vertical"] = InputAxisBinding {
            .negativeKeys   = {fra::KeyCode::S, fra::KeyCode::Down},
            .positiveKeys   = {fra::KeyCode::W, fra::KeyCode::Up},
            .gamepadAxis    = fra::GamepadAxis::GamepadAxisLeftY,
            .deadzone       = 0.15f,
            .scale          = 1.0f,
            .invertGamepad  = true,
        };
        map.axes["LookX"] = InputAxisBinding {
            .negativeKeys   = {},
            .positiveKeys   = {},
            .gamepadAxis    = fra::GamepadAxis::GamepadAxisRightX,
            .mouseAxis      = MouseMotionAxis::DeltaX,
            .deadzone       = 0.15f,
            .scale          = 180.0f,
            .mouseScale     = 0.15f,
            .invertGamepad  = false,
            .invertMouse    = false,
        };
        map.axes["LookY"] = InputAxisBinding {
            .negativeKeys   = {},
            .positiveKeys   = {},
            .gamepadAxis    = fra::GamepadAxis::GamepadAxisRightY,
            .mouseAxis      = MouseMotionAxis::DeltaY,
            .deadzone       = 0.15f,
            .scale          = 180.0f,
            .mouseScale     = 0.15f,
            .invertGamepad  = true,
            .invertMouse    = true,
        };
        map.axes["Zoom"] = InputAxisBinding {
            .negativeKeys = {},
            .positiveKeys = {},
            .mouseAxis    = MouseMotionAxis::Scroll,
            .mouseScale   = 1.0f,
        };

        return map;
    }

} // namespace FRIGGA_NAMESPACE
