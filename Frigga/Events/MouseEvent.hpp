#pragma once

#include "Event.hpp"

FRIGGA_BEGIN

class MouseMoveEvent: public Event
{
  public:
    MouseMoveEvent(float mouseX, float mouseY): m_mouseX(mouseX), m_mouseY(mouseY) {}

    inline float getMouseX() const
    {
        return m_mouseX;
    }

    inline float getMouseY() const
    {
        return m_mouseY;
    }

    inline glm::vec2 getMousePos() const
    {
        return {m_mouseX, m_mouseY};
    }

    std::string toString() const override
    {
        std::stringstream ss;
        ss << "MouseMoveEvent: " << getMouseX() << ", " << getMouseY();
        return ss.str();
    }

    EVENT_CLASS_TYPE(MouseMoved)
    EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)
  private:
    float m_mouseX, m_mouseY;
};

class MouseScrolledEvent: public Event
{
  public:
    MouseScrolledEvent(float xOffset, float yOffset): m_xOffset(xOffset), m_yOffset(yOffset) {}

    inline float getXOffset() const
    {
        return m_xOffset;
    }
    inline float getYOffset() const
    {
        return m_yOffset;
    }

    std::string toString() const override
    {
        std::stringstream ss;
        ss << "MouseScrolledEvent: " << getXOffset() << ", " << getXOffset();
        return ss.str();
    }

    EVENT_CLASS_TYPE(MouseScrolled)
    EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)
  private:
    float m_xOffset, m_yOffset;
};

class MouseButtonEvent: public Event
{
  public:
    inline int GetMouseButton() const
    {
        return m_button;
    }

    EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)
  protected:
    MouseButtonEvent(int button): m_button(button) {}

    int m_button;
};

class MouseButtonPressedEvent: public MouseButtonEvent
{
  public:
    MouseButtonPressedEvent(int button): MouseButtonEvent(button) {}

    std::string toString() const override
    {
        std::stringstream ss;
        ss << "MouseButtonPressedEvent: " << m_button;
        return ss.str();
    }

    EVENT_CLASS_TYPE(MouseButtonPressed)
};

class MouseButtonReleasedEvent: public MouseButtonEvent
{
  public:
    MouseButtonReleasedEvent(int button): MouseButtonEvent(button) {}

    std::string toString() const override
    {
        std::stringstream ss;
        ss << "MouseButtonReleasedEvent: " << m_button;
        return ss.str();
    }

    EVENT_CLASS_TYPE(MouseButtonReleased)
};

FRIGGA_END
