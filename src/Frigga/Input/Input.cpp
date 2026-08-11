#include "Frigga/Input/Input.hpp"

#include "Frigga/Scene/SceneSimulationState.hpp"

#include <Freya/Events/EventManager.hpp>
#include <Freya/Events/Gamepad.hpp>
#include <Freya/Events/Keyboard.hpp>
#include <Freya/Events/Mouse.hpp>

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace FRIGGA_NAMESPACE
{

    Input::Input(const skr::Arc<fra::EventManager> &events,
                 const skr::Arc<SceneSimulationState> &simulation)
        : mEvents(events), mSimulation(simulation)
    {
        ResetToDefaults();
        subscribe();
    }

    Input::~Input() = default;

    void Input::subscribe()
    {
        if(!mEvents)
        {
            return;
        }

        mEvents->Subscribe<fra::KeyPressedEvent>([this](const fra::KeyPressedEvent &event) {
            mKeysDown.insert(event.key);
        });
        mEvents->Subscribe<fra::KeyReleasedEvent>([this](const fra::KeyReleasedEvent &event) {
            mKeysDown.erase(event.key);
        });
        mEvents->Subscribe<fra::MouseButtonPressedEvent>(
            [this](const fra::MouseButtonPressedEvent &event) { mMouseDown.insert(event.button); });
        mEvents->Subscribe<fra::MouseButtonReleasedEvent>(
            [this](const fra::MouseButtonReleasedEvent &event) { mMouseDown.erase(event.button); });
        mEvents->Subscribe<fra::GamepadButtonPressedEvent>(
            [this](const fra::GamepadButtonPressedEvent &event) {
                mGamepadDown.insert(event.button);
            });
        mEvents->Subscribe<fra::GamepadButtonReleasedEvent>(
            [this](const fra::GamepadButtonReleasedEvent &event) {
                mGamepadDown.erase(event.button);
            });
        mEvents->Subscribe<fra::GamepadAxisMotionEvent>(
            [this](const fra::GamepadAxisMotionEvent &event) {
                mGamepadAxes[event.axis] = static_cast<float>(event.value);
            });
    }

    bool Input::IsDown(std::string_view action) const
    {
        const auto it = mActions.find(std::string(action));
        return it != mActions.end() && it->second.down;
    }

    bool Input::WasPressed(std::string_view action) const
    {
        const auto it = mActions.find(std::string(action));
        return it != mActions.end() && it->second.pressed;
    }

    bool Input::WasReleased(std::string_view action) const
    {
        const auto it = mActions.find(std::string(action));
        return it != mActions.end() && it->second.released;
    }

    float Input::GetAxis(std::string_view axis) const
    {
        const auto it = mAxes.find(std::string(axis));
        return it != mAxes.end() ? it->second : 0.0f;
    }

    void Input::BeginFrame()
    {
        const bool playing = mSimulation && mSimulation->IsPlaying();
        const bool running = mSimulation && mSimulation->IsRunning();

        if(!playing || !running)
        {
            for(auto &[name, state] : mActions)
            {
                state.pressed  = false;
                state.released = state.down;
                state.down     = false;
            }
            for(auto &[name, value] : mAxes)
            {
                (void)name;
                value = 0.0f;
            }
            // Ensure axes map has entries for all bindings when cleared.
            for(const auto &[name, binding] : mMap.axes)
            {
                (void)binding;
                mAxes[name] = 0.0f;
            }
            return;
        }

        for(const auto &[name, binding] : mMap.actions)
        {
            const bool raw = evaluateAction(binding);
            auto &state    = mActions[name];
            state.pressed  = raw && !state.down;
            state.released = !raw && state.down;
            state.down     = raw;
        }

        for(const auto &[name, binding] : mMap.axes)
        {
            mAxes[name] = evaluateAxis(binding);
        }
    }

    void Input::LoadBindings(const InputMap &map)
    {
        mMap = map;
        mActions.clear();
        mAxes.clear();
        for(const auto &[name, binding] : mMap.actions)
        {
            (void)binding;
            mActions[name] = {};
        }
        for(const auto &[name, binding] : mMap.axes)
        {
            (void)binding;
            mAxes[name] = 0.0f;
        }
    }

    void Input::ResetToDefaults()
    {
        LoadBindings(MakeDefaultInputMap());
    }

    void Input::InjectKey(fra::KeyCode key, bool down)
    {
        if(down)
        {
            mKeysDown.insert(key);
        }
        else
        {
            mKeysDown.erase(key);
        }
    }

    void Input::InjectMouseButton(fra::MouseButton button, bool down)
    {
        if(down)
        {
            mMouseDown.insert(button);
        }
        else
        {
            mMouseDown.erase(button);
        }
    }

    void Input::InjectGamepadButton(fra::GamepadButton button, bool down)
    {
        if(down)
        {
            mGamepadDown.insert(button);
        }
        else
        {
            mGamepadDown.erase(button);
        }
    }

    void Input::InjectGamepadAxis(fra::GamepadAxis axis, float value)
    {
        mGamepadAxes[axis] = value;
    }

    bool Input::keyboardMouseAllowed() const
    {
        if(mGameplayViewportHovered)
        {
            return true;
        }

        if(ImGui::GetCurrentContext() == nullptr)
        {
            return true;
        }

        const ImGuiIO &io = ImGui::GetIO();
        if(io.WantCaptureKeyboard || io.WantCaptureMouse)
        {
            return false;
        }
        return true;
    }

    bool Input::isKeyDown(fra::KeyCode key) const
    {
        return mKeysDown.contains(key);
    }

    bool Input::isMouseDown(fra::MouseButton button) const
    {
        return mMouseDown.contains(button);
    }

    bool Input::isGamepadDown(fra::GamepadButton button) const
    {
        return mGamepadDown.contains(button);
    }

    float Input::gamepadAxisValue(fra::GamepadAxis axis) const
    {
        const auto it = mGamepadAxes.find(axis);
        return it != mGamepadAxes.end() ? it->second : 0.0f;
    }

    bool Input::evaluateAction(const InputActionBinding &binding) const
    {
        const bool allowKm = keyboardMouseAllowed();
        if(allowKm)
        {
            for(const auto key : binding.keys)
            {
                if(isKeyDown(key))
                {
                    return true;
                }
            }
            for(const auto button : binding.mouseButtons)
            {
                if(isMouseDown(button))
                {
                    return true;
                }
            }
        }

        for(const auto button : binding.gamepadButtons)
        {
            if(isGamepadDown(button))
            {
                return true;
            }
        }
        return false;
    }

    float Input::evaluateAxis(const InputAxisBinding &binding) const
    {
        float value = 0.0f;
        const bool allowKm = keyboardMouseAllowed();
        if(allowKm)
        {
            bool neg = false;
            bool pos = false;
            for(const auto key : binding.negativeKeys)
            {
                neg = neg || isKeyDown(key);
            }
            for(const auto key : binding.positiveKeys)
            {
                pos = pos || isKeyDown(key);
            }
            if(pos)
            {
                value += 1.0f;
            }
            if(neg)
            {
                value -= 1.0f;
            }
        }

        if(binding.gamepadAxis)
        {
            float stick = gamepadAxisValue(*binding.gamepadAxis);
            if(binding.invertGamepad)
            {
                stick = -stick;
            }
            if(std::abs(stick) < binding.deadzone)
            {
                stick = 0.0f;
            }
            // Prefer stick when it has magnitude; otherwise keep keyboard composite.
            if(std::abs(stick) > std::abs(value))
            {
                value = stick;
            }
        }

        value *= binding.scale;
        return std::clamp(value, -1.0f, 1.0f);
    }

} // namespace FRIGGA_NAMESPACE
