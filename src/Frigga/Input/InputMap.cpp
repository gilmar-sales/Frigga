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

        return map;
    }

} // namespace FRIGGA_NAMESPACE
