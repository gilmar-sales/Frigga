#include "Frigga/Input/InputMapIO.hpp"

#include <simdjson.h>

#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        template <typename E>
        using EnumMap = std::unordered_map<std::string_view, E>;

        const EnumMap<fra::KeyCode> &KeyTable()
        {
            static const EnumMap<fra::KeyCode> table = {
                {"Unknown", fra::KeyCode::Unknown},
                {"A", fra::KeyCode::A},
                {"B", fra::KeyCode::B},
                {"C", fra::KeyCode::C},
                {"D", fra::KeyCode::D},
                {"E", fra::KeyCode::E},
                {"F", fra::KeyCode::F},
                {"G", fra::KeyCode::G},
                {"H", fra::KeyCode::H},
                {"I", fra::KeyCode::I},
                {"J", fra::KeyCode::J},
                {"K", fra::KeyCode::K},
                {"L", fra::KeyCode::L},
                {"M", fra::KeyCode::M},
                {"N", fra::KeyCode::N},
                {"O", fra::KeyCode::O},
                {"P", fra::KeyCode::P},
                {"Q", fra::KeyCode::Q},
                {"R", fra::KeyCode::R},
                {"S", fra::KeyCode::S},
                {"T", fra::KeyCode::T},
                {"U", fra::KeyCode::U},
                {"V", fra::KeyCode::V},
                {"W", fra::KeyCode::W},
                {"X", fra::KeyCode::X},
                {"Y", fra::KeyCode::Y},
                {"Z", fra::KeyCode::Z},
                {"Space", fra::KeyCode::Space},
                {"Return", fra::KeyCode::Return},
                {"Escape", fra::KeyCode::Escape},
                {"Tab", fra::KeyCode::Tab},
                {"Backspace", fra::KeyCode::Backspace},
                {"Left", fra::KeyCode::Left},
                {"Right", fra::KeyCode::Right},
                {"Up", fra::KeyCode::Up},
                {"Down", fra::KeyCode::Down},
                {"LShift", fra::KeyCode::LShift},
                {"RShift", fra::KeyCode::RShift},
                {"LCtrl", fra::KeyCode::LCtrl},
                {"RCtrl", fra::KeyCode::RCtrl},
                {"LAlt", fra::KeyCode::LAlt},
                {"RAlt", fra::KeyCode::RAlt},
            };
            return table;
        }

        const EnumMap<fra::MouseButton> &MouseTable()
        {
            static const EnumMap<fra::MouseButton> table = {
                {"Left", fra::MouseButton::Left},
                {"Middle", fra::MouseButton::Middle},
                {"Right", fra::MouseButton::Right},
                {"Button4", fra::MouseButton::Button4},
                {"Button5", fra::MouseButton::Button5},
            };
            return table;
        }

        const EnumMap<fra::GamepadButton> &GamepadButtonTable()
        {
            static const EnumMap<fra::GamepadButton> table = {
                {"South", fra::GamepadButton::GamepadButtonSouth},
                {"East", fra::GamepadButton::GamepadButtonEast},
                {"West", fra::GamepadButton::GamepadButtonWest},
                {"North", fra::GamepadButton::GamepadButtonNorth},
                {"Back", fra::GamepadButton::GamepadButtonBack},
                {"Guide", fra::GamepadButton::GamepadButtonGuide},
                {"Start", fra::GamepadButton::GamepadButtonStart},
                {"LeftStick", fra::GamepadButton::GamepadButtonLeftStick},
                {"RightStick", fra::GamepadButton::GamepadButtonRightStick},
                {"LeftShoulder", fra::GamepadButton::GamepadButtonLeftShoulder},
                {"RightShoulder", fra::GamepadButton::GamepadButtonRightShoulder},
                {"DpadUp", fra::GamepadButton::GamepadButtonDpadUp},
                {"DpadDown", fra::GamepadButton::GamepadButtonDpadDown},
                {"DpadLeft", fra::GamepadButton::GamepadButtonDpadLeft},
                {"DpadRight", fra::GamepadButton::GamepadButtonDpadRight},
            };
            return table;
        }

        const EnumMap<fra::GamepadAxis> &GamepadAxisTable()
        {
            static const EnumMap<fra::GamepadAxis> table = {
                {"LeftX", fra::GamepadAxis::GamepadAxisLeftX},
                {"LeftY", fra::GamepadAxis::GamepadAxisLeftY},
                {"RightX", fra::GamepadAxis::GamepadAxisRightX},
                {"RightY", fra::GamepadAxis::GamepadAxisRightY},
                {"LeftTrigger", fra::GamepadAxis::GamepadAxisLeftTrigger},
                {"RightTrigger", fra::GamepadAxis::GamepadAxisRightTrigger},
            };
            return table;
        }

        template <typename E>
        std::optional<E> Lookup(const EnumMap<E> &table, std::string_view name)
        {
            const auto it = table.find(name);
            if(it == table.end())
            {
                return std::nullopt;
            }
            return it->second;
        }

        template <typename E>
        std::string_view ReverseLookup(const EnumMap<E> &table, E value)
        {
            for(const auto &[name, enumValue] : table)
            {
                if(enumValue == value)
                {
                    return name;
                }
            }
            return {};
        }

        void AppendStringArray(std::ostringstream &out, const char *key,
                               const std::vector<std::string> &values)
        {
            out << "    \"" << key << "\": [";
            for(std::size_t i = 0; i < values.size(); ++i)
            {
                if(i > 0)
                {
                    out << ", ";
                }
                out << '"' << values[i] << '"';
            }
            out << ']';
        }

        bool ReadStringArray(simdjson::ondemand::value value, std::vector<std::string> &out,
                             std::string *error)
        {
            out.clear();
            simdjson::ondemand::array arr;
            if(value.get_array().get(arr))
            {
                if(error)
                {
                    *error = "expected string array";
                }
                return false;
            }
            for(auto element : arr)
            {
                std::string_view text;
                if(element.get_string().get(text))
                {
                    if(error)
                    {
                        *error = "expected string in array";
                    }
                    return false;
                }
                out.emplace_back(text);
            }
            return true;
        }
    } // namespace

    std::optional<fra::KeyCode> ParseKeyName(std::string_view name)
    {
        return Lookup(KeyTable(), name);
    }

    std::optional<fra::MouseButton> ParseMouseButtonName(std::string_view name)
    {
        return Lookup(MouseTable(), name);
    }

    std::optional<fra::GamepadButton> ParseGamepadButtonName(std::string_view name)
    {
        return Lookup(GamepadButtonTable(), name);
    }

    std::optional<fra::GamepadAxis> ParseGamepadAxisName(std::string_view name)
    {
        return Lookup(GamepadAxisTable(), name);
    }

    std::string_view KeyName(fra::KeyCode key)
    {
        return ReverseLookup(KeyTable(), key);
    }

    std::string_view MouseButtonName(fra::MouseButton button)
    {
        return ReverseLookup(MouseTable(), button);
    }

    std::string_view GamepadButtonName(fra::GamepadButton button)
    {
        return ReverseLookup(GamepadButtonTable(), button);
    }

    std::string_view GamepadAxisName(fra::GamepadAxis axis)
    {
        return ReverseLookup(GamepadAxisTable(), axis);
    }

    std::span<const std::string_view> KnownKeyNames()
    {
        static constexpr std::string_view names[] = {
            "A",         "B",         "C",         "D",         "E",         "F",
            "G",         "H",         "I",         "J",         "K",         "L",
            "M",         "N",         "O",         "P",         "Q",         "R",
            "S",         "T",         "U",         "V",         "W",         "X",
            "Y",         "Z",         "Space",     "Return",    "Escape",    "Tab",
            "Backspace", "Left",      "Right",     "Up",        "Down",      "LShift",
            "RShift",    "LCtrl",     "RCtrl",     "LAlt",      "RAlt",
        };
        return names;
    }

    std::span<const std::string_view> KnownMouseButtonNames()
    {
        static constexpr std::string_view names[] = {
            "Left", "Middle", "Right", "Button4", "Button5",
        };
        return names;
    }

    std::span<const std::string_view> KnownGamepadButtonNames()
    {
        static constexpr std::string_view names[] = {
            "South",         "East",          "West",           "North",
            "Back",          "Guide",         "Start",          "LeftStick",
            "RightStick",    "LeftShoulder",  "RightShoulder",  "DpadUp",
            "DpadDown",      "DpadLeft",      "DpadRight",
        };
        return names;
    }

    std::span<const std::string_view> KnownGamepadAxisNames()
    {
        static constexpr std::string_view names[] = {
            "LeftX", "LeftY", "RightX", "RightY", "LeftTrigger", "RightTrigger",
        };
        return names;
    }

    std::string SerializeInputMap(const InputMap &map)
    {
        std::ostringstream out;
        out << "{\n";
        out << "  \"version\": " << map.version << ",\n";
        out << "  \"actions\": {\n";
        bool firstAction = true;
        for(const auto &[name, action] : map.actions)
        {
            if(!firstAction)
            {
                out << ",\n";
            }
            firstAction = false;
            out << "    \"" << name << "\": {\n";

            std::vector<std::string> keys;
            for(const auto key : action.keys)
            {
                if(const auto label = KeyName(key); !label.empty())
                {
                    keys.emplace_back(label);
                }
            }
            std::vector<std::string> mouse;
            for(const auto button : action.mouseButtons)
            {
                if(const auto label = MouseButtonName(button); !label.empty())
                {
                    mouse.emplace_back(label);
                }
            }
            std::vector<std::string> pads;
            for(const auto button : action.gamepadButtons)
            {
                if(const auto label = GamepadButtonName(button); !label.empty())
                {
                    pads.emplace_back(label);
                }
            }

            AppendStringArray(out, "keys", keys);
            out << ",\n";
            AppendStringArray(out, "mouseButtons", mouse);
            out << ",\n";
            AppendStringArray(out, "gamepadButtons", pads);
            out << "\n    }";
        }
        out << "\n  },\n";
        out << "  \"axes\": {\n";
        bool firstAxis = true;
        for(const auto &[name, axis] : map.axes)
        {
            if(!firstAxis)
            {
                out << ",\n";
            }
            firstAxis = false;
            out << "    \"" << name << "\": {\n";

            std::vector<std::string> neg;
            for(const auto key : axis.negativeKeys)
            {
                if(const auto label = KeyName(key); !label.empty())
                {
                    neg.emplace_back(label);
                }
            }
            std::vector<std::string> pos;
            for(const auto key : axis.positiveKeys)
            {
                if(const auto label = KeyName(key); !label.empty())
                {
                    pos.emplace_back(label);
                }
            }

            AppendStringArray(out, "negativeKeys", neg);
            out << ",\n";
            AppendStringArray(out, "positiveKeys", pos);
            out << ",\n";
            if(axis.gamepadAxis)
            {
                out << "    \"gamepadAxis\": \"" << GamepadAxisName(*axis.gamepadAxis) << "\",\n";
            }
            out << "    \"deadzone\": " << axis.deadzone << ",\n";
            out << "    \"scale\": " << axis.scale << ",\n";
            out << "    \"invertGamepad\": " << (axis.invertGamepad ? "true" : "false") << "\n";
            out << "    }";
        }
        out << "\n  }\n";
        out << "}\n";
        return out.str();
    }

    bool ParseInputMap(std::string_view json, InputMap &out, std::string *error)
    {
        InputMap parsed;
        parsed.version = 1;

        simdjson::ondemand::parser parser;
        simdjson::padded_string padded(json);
        simdjson::ondemand::document doc;
        if(parser.iterate(padded).get(doc))
        {
            if(error)
            {
                *error = "invalid JSON";
            }
            return false;
        }

        auto root = doc.get_object();
        if(root.error())
        {
            if(error)
            {
                *error = "expected JSON object";
            }
            return false;
        }

        auto versionField = root["version"];
        if(!versionField.error())
        {
            int64_t version = 1;
            if(!versionField.get_int64().get(version))
            {
                parsed.version = static_cast<int>(version);
            }
        }

        auto actionsField = root["actions"];
        if(!actionsField.error())
        {
            simdjson::ondemand::object actionsObj;
            if(actionsField.get_object().get(actionsObj))
            {
                if(error)
                {
                    *error = "actions must be an object";
                }
                return false;
            }
            for(auto field : actionsObj)
            {
                std::string_view actionName;
                if(field.unescaped_key().get(actionName))
                {
                    continue;
                }
                simdjson::ondemand::object actionObj;
                if(field.value().get_object().get(actionObj))
                {
                    if(error)
                    {
                        *error = "action binding must be an object";
                    }
                    return false;
                }

                InputActionBinding binding;
                std::vector<std::string> keys;
                std::vector<std::string> mouse;
                std::vector<std::string> pads;

                auto keysField = actionObj["keys"];
                if(!keysField.error() && !ReadStringArray(keysField.value(), keys, error))
                {
                    return false;
                }
                auto mouseField = actionObj["mouseButtons"];
                if(!mouseField.error() && !ReadStringArray(mouseField.value(), mouse, error))
                {
                    return false;
                }
                auto padField = actionObj["gamepadButtons"];
                if(!padField.error() && !ReadStringArray(padField.value(), pads, error))
                {
                    return false;
                }

                for(const auto &name : keys)
                {
                    if(const auto key = ParseKeyName(name))
                    {
                        binding.keys.push_back(*key);
                    }
                }
                for(const auto &name : mouse)
                {
                    if(const auto button = ParseMouseButtonName(name))
                    {
                        binding.mouseButtons.push_back(*button);
                    }
                }
                for(const auto &name : pads)
                {
                    if(const auto button = ParseGamepadButtonName(name))
                    {
                        binding.gamepadButtons.push_back(*button);
                    }
                }
                parsed.actions.emplace(std::string(actionName), std::move(binding));
            }
        }

        auto axesField = root["axes"];
        if(!axesField.error())
        {
            simdjson::ondemand::object axesObj;
            if(axesField.get_object().get(axesObj))
            {
                if(error)
                {
                    *error = "axes must be an object";
                }
                return false;
            }
            for(auto field : axesObj)
            {
                std::string_view axisName;
                if(field.unescaped_key().get(axisName))
                {
                    continue;
                }
                simdjson::ondemand::object axisObj;
                if(field.value().get_object().get(axisObj))
                {
                    if(error)
                    {
                        *error = "axis binding must be an object";
                    }
                    return false;
                }

                InputAxisBinding binding;
                std::vector<std::string> neg;
                std::vector<std::string> pos;

                auto negField = axisObj["negativeKeys"];
                if(!negField.error() && !ReadStringArray(negField.value(), neg, error))
                {
                    return false;
                }
                auto posField = axisObj["positiveKeys"];
                if(!posField.error() && !ReadStringArray(posField.value(), pos, error))
                {
                    return false;
                }
                for(const auto &name : neg)
                {
                    if(const auto key = ParseKeyName(name))
                    {
                        binding.negativeKeys.push_back(*key);
                    }
                }
                for(const auto &name : pos)
                {
                    if(const auto key = ParseKeyName(name))
                    {
                        binding.positiveKeys.push_back(*key);
                    }
                }

                auto gamepadAxisField = axisObj["gamepadAxis"];
                if(!gamepadAxisField.error())
                {
                    std::string_view axisLabel;
                    if(!gamepadAxisField.get_string().get(axisLabel))
                    {
                        binding.gamepadAxis = ParseGamepadAxisName(axisLabel);
                    }
                }

                auto deadzoneField = axisObj["deadzone"];
                if(!deadzoneField.error())
                {
                    double deadzone = binding.deadzone;
                    if(!deadzoneField.get_double().get(deadzone))
                    {
                        binding.deadzone = static_cast<float>(deadzone);
                    }
                }
                auto scaleField = axisObj["scale"];
                if(!scaleField.error())
                {
                    double scale = binding.scale;
                    if(!scaleField.get_double().get(scale))
                    {
                        binding.scale = static_cast<float>(scale);
                    }
                }
                auto invertField = axisObj["invertGamepad"];
                if(!invertField.error())
                {
                    bool invert = binding.invertGamepad;
                    if(!invertField.get_bool().get(invert))
                    {
                        binding.invertGamepad = invert;
                    }
                }

                parsed.axes.emplace(std::string(axisName), std::move(binding));
            }
        }

        out = std::move(parsed);
        return true;
    }

    bool LoadInputMapFile(const std::filesystem::path &path, InputMap &out, std::string *error)
    {
        simdjson::padded_string json;
        if(const auto err = simdjson::padded_string::load(path.string()).get(json); err)
        {
            if(error)
            {
                *error = simdjson::error_message(err);
            }
            return false;
        }
        return ParseInputMap(json, out, error);
    }

    bool SaveInputMapFile(const std::filesystem::path &path, const InputMap &map, std::string *error)
    {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream file(path);
        if(!file)
        {
            if(error)
            {
                *error = "failed to open " + path.string();
            }
            return false;
        }
        file << SerializeInputMap(map);
        return static_cast<bool>(file);
    }

} // namespace FRIGGA_NAMESPACE
