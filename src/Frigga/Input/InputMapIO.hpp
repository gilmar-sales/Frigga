#pragma once

#include "Frigga/Input/InputMap.hpp"

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace FRIGGA_NAMESPACE
{

    [[nodiscard]] std::optional<fra::KeyCode> ParseKeyName(std::string_view name);
    [[nodiscard]] std::optional<fra::MouseButton> ParseMouseButtonName(std::string_view name);
    [[nodiscard]] std::optional<fra::GamepadButton> ParseGamepadButtonName(std::string_view name);
    [[nodiscard]] std::optional<fra::GamepadAxis> ParseGamepadAxisName(std::string_view name);
    [[nodiscard]] std::optional<MouseMotionAxis> ParseMouseAxisName(std::string_view name);

    [[nodiscard]] std::string_view KeyName(fra::KeyCode key);
    [[nodiscard]] std::string_view MouseButtonName(fra::MouseButton button);
    [[nodiscard]] std::string_view GamepadButtonName(fra::GamepadButton button);
    [[nodiscard]] std::string_view GamepadAxisName(fra::GamepadAxis axis);
    [[nodiscard]] std::string_view MouseAxisName(MouseMotionAxis axis);

    /// Ordered labels for editor combos (same tokens as JSON).
    [[nodiscard]] std::span<const std::string_view> KnownKeyNames();
    [[nodiscard]] std::span<const std::string_view> KnownMouseButtonNames();
    [[nodiscard]] std::span<const std::string_view> KnownGamepadButtonNames();
    [[nodiscard]] std::span<const std::string_view> KnownGamepadAxisNames();
    [[nodiscard]] std::span<const std::string_view> KnownMouseAxisNames();

    [[nodiscard]] std::string SerializeInputMap(const InputMap &map);
    [[nodiscard]] bool ParseInputMap(std::string_view json, InputMap &out, std::string *error = nullptr);
    [[nodiscard]] bool LoadInputMapFile(const std::filesystem::path &path, InputMap &out,
                                        std::string *error = nullptr);
    [[nodiscard]] bool SaveInputMapFile(const std::filesystem::path &path, const InputMap &map,
                                        std::string *error = nullptr);

} // namespace FRIGGA_NAMESPACE
