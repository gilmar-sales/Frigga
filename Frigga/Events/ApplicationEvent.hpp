#pragma once

#include "Event.hpp"

FRIGGA_BEGIN

class WindowResizeEvent: public Event
{
  public:
    WindowResizeEvent(unsigned width, unsigned height): m_width(width), m_height(height) {}

    inline unsigned getWidth() const
    {
        return m_width;
    }
    inline unsigned getHeight() const
    {
        return m_height;
    }

    std::string toString() const override
    {
        std::stringstream ss;
        ss << "WindowResizeEvent: " << getWidth() << ", " << getHeight();
        return ss.str();
    }

    EVENT_CLASS_TYPE(WindowResize)
    EVENT_CLASS_CATEGORY(EventCategoryApplication)

  private:
    unsigned m_width, m_height;
};

class WindowCloseEvent: public Event
{
  public:
    WindowCloseEvent() {}

    EVENT_CLASS_TYPE(WindowClose)
    EVENT_CLASS_CATEGORY(EventCategoryApplication)
};

class AppTickEvent: public Event
{
  public:
    AppTickEvent() {}

    EVENT_CLASS_TYPE(AppTick)
    EVENT_CLASS_CATEGORY(EventCategoryApplication)
};

class AppUpdateEvent: public Event
{
    AppUpdateEvent() {}

    EVENT_CLASS_TYPE(AppUpdate)
    EVENT_CLASS_CATEGORY(EventCategoryApplication)
};

FRIGGA_END