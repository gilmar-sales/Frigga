#pragma once

#ifndef FREYA_NAMESPACE
#    define FREYA_NAMESPACE fra
#endif

#include <Freya/Events/Gamepad.hpp>
#include <Freya/Events/KeyCode.hpp>
#include <Freya/Events/Mouse.hpp>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace FRIGGA_NAMESPACE
{

    struct InputActionBinding
    {
        std::vector<fra::KeyCode> keys;
        std::vector<fra::MouseButton> mouseButtons;
        std::vector<fra::GamepadButton> gamepadButtons;
    };

    struct InputAxisBinding
    {
        std::vector<fra::KeyCode> negativeKeys;
        std::vector<fra::KeyCode> positiveKeys;
        std::optional<fra::GamepadAxis> gamepadAxis;
        float deadzone = 0.15f;
        float scale    = 1.0f;
        bool invertGamepad = false;
    };

    struct InputMap
    {
        int version = 1;
        std::unordered_map<std::string, InputActionBinding> actions;
        std::unordered_map<std::string, InputAxisBinding> axes;
    };

    [[nodiscard]] InputMap MakeDefaultInputMap();

} // namespace FRIGGA_NAMESPACE
