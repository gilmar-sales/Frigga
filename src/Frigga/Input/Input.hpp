#pragma once

#include "Frigga/Input/InputMap.hpp"

#include <Skirnir/Skirnir.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#ifndef FREYA_NAMESPACE
#    define FREYA_NAMESPACE fra
#endif

namespace FREYA_NAMESPACE
{
    class EventManager;
}

namespace FRIGGA_NAMESPACE
{
    class SceneSimulationState;

    class Input
    {
      public:
        Input(const skr::Arc<fra::EventManager> &events,
              const skr::Arc<SceneSimulationState> &simulation);
        ~Input();

        Input(const Input &)            = delete;
        Input &operator=(const Input &) = delete;

        [[nodiscard]] bool IsDown(std::string_view action) const;
        [[nodiscard]] bool WasPressed(std::string_view action) const;
        [[nodiscard]] bool WasReleased(std::string_view action) const;
        [[nodiscard]] float GetAxis(std::string_view axis) const;

        /// Recompute action edges + axes for this frame (call before Freyr Update).
        void BeginFrame(float deltaTime = 1.0f / 60.0f);

        void LoadBindings(const InputMap &map);
        void ResetToDefaults();

        void SetGameplayViewportHovered(bool hovered)
        {
            mGameplayViewportHovered = hovered;
        }

        /// Test / tooling: inject device state without SDL.
        void InjectKey(fra::KeyCode key, bool down);
        void InjectMouseButton(fra::MouseButton button, bool down);
        void InjectGamepadButton(fra::GamepadButton button, bool down);
        void InjectGamepadAxis(fra::GamepadAxis axis, float value);
        void InjectMouseDelta(float deltaX, float deltaY);
        void InjectMouseScroll(float scroll);

        [[nodiscard]] const InputMap &GetBindings() const
        {
            return mMap;
        }

      private:
        struct EnumHash
        {
            template <typename E>
            std::size_t operator()(E value) const noexcept
            {
                return static_cast<std::size_t>(value);
            }
        };

        struct ActionFrame
        {
            bool down     = false;
            bool pressed  = false;
            bool released = false;
        };

        void subscribe();
        [[nodiscard]] bool keyboardMouseAllowed() const;
        [[nodiscard]] bool evaluateAction(const InputActionBinding &binding) const;
        [[nodiscard]] float evaluateAxis(const InputAxisBinding &binding, float deltaTime) const;
        [[nodiscard]] bool isKeyDown(fra::KeyCode key) const;
        [[nodiscard]] bool isMouseDown(fra::MouseButton button) const;
        [[nodiscard]] bool isGamepadDown(fra::GamepadButton button) const;
        [[nodiscard]] float gamepadAxisValue(fra::GamepadAxis axis) const;
        [[nodiscard]] float mouseAxisValue(MouseMotionAxis axis) const;

        skr::Arc<fra::EventManager> mEvents;
        skr::Arc<SceneSimulationState> mSimulation;
        InputMap mMap {};

        std::unordered_set<fra::KeyCode, EnumHash> mKeysDown;
        std::unordered_set<fra::MouseButton, EnumHash> mMouseDown;
        std::unordered_set<fra::GamepadButton, EnumHash> mGamepadDown;
        std::unordered_map<fra::GamepadAxis, float, EnumHash> mGamepadAxes;

        std::unordered_map<std::string, ActionFrame> mActions;
        std::unordered_map<std::string, float> mAxes;

        float mMouseDeltaX = 0.0f;
        float mMouseDeltaY = 0.0f;
        float mMouseScroll = 0.0f;

        bool mGameplayViewportHovered = false;
    };

} // namespace FRIGGA_NAMESPACE
