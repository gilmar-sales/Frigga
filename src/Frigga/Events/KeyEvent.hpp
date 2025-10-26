#pragma once

#include "Event.hpp"

namespace FRIGGA_NAMESPACE
{

    class KeyEvent: public Event
    {
      public:
        inline int getKeyCode()
        {
            return m_keyCode;
        }

        EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput);

      protected:
        KeyEvent(int keyCode): m_keyCode(keyCode) {}

        int m_keyCode;
    };

    class KeyPressedEvent: public KeyEvent
    {
      public:
        KeyPressedEvent(int keyCode, int repeatCount): KeyEvent(keyCode), m_repeatCount(repeatCount)
        {
        }

        inline int GetRepeatCount()
        {
            return m_repeatCount;
        }

        std::string toString() const override
        {
            std::stringstream ss;
            ss << "KeyPressedEvent: " << m_keyCode << "(" << m_repeatCount << " repeats)";
            return ss.str();
        }

        EVENT_CLASS_TYPE(KeyPressed)
      private:
        int m_repeatCount;
    };

    class KeyReleasedEvent: public KeyEvent
    {
      public:
        KeyReleasedEvent(int keycode): KeyEvent(keycode) {}

        std::string toString() const override
        {
            std::stringstream ss;
            ss << "KeyReleasedEvent: " << m_keyCode;
            return ss.str();
        }

        EVENT_CLASS_TYPE(KeyReleased)
    };

    class KeyTypedEvent: public KeyEvent
    {
      public:
        KeyTypedEvent(int keycode): KeyEvent(keycode) {}

        std::string toString() const override
        {
            std::stringstream ss;
            ss << "KeyTypedEvent: " << m_keyCode;
            return ss.str();
        }

        EVENT_CLASS_TYPE(KeyTyped)
    };

} // namespace FRIGGA_NAMESPACE
