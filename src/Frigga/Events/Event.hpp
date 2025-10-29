#pragma once

#include <functional>

namespace FRIGGA_NAMESPACE
{

    enum class EventType
    {
        None = 0,
        WindowClose,
        WindowResize,
        WindowFocus,
        WindowLostFocus,
        WindowMoved,
        AppTick,
        AppUpdate,
        AppRender,
        KeyPressed,
        KeyReleased,
        KeyTyped,
        MouseButtonPressed,
        MouseButtonReleased,
        MouseMoved,
        MouseScrolled
    };

    enum EventCategory
    {
        None                     = 0,
        EventCategoryApplication = BIT(0),
        EventCategoryInput       = BIT(1),
        EventCategoryKeyboard    = BIT(2),
        EventCategoryMouse       = BIT(3),
        EventCategoryMouseButton = BIT(4)
    };

#define EVENT_CLASS_TYPE(type)                                                                     \
    static EventType getStaticType()                                                               \
    {                                                                                              \
        return EventType::type;                                                                    \
    }                                                                                              \
    virtual EventType getEventType() const override                                                \
    {                                                                                              \
        return getStaticType();                                                                    \
    }                                                                                              \
    virtual const char *getName() const override                                                   \
    {                                                                                              \
        return #type;                                                                              \
    }

#define EVENT_CLASS_CATEGORY(category)                                                             \
    virtual int getCategoryFlags() const override                                                  \
    {                                                                                              \
        return category;                                                                           \
    }

    class Event
    {
        friend class AbstractApplication;
        friend class EventDispatcher;

      public:
        virtual EventType getEventType() const = 0;
        virtual const char *getName() const    = 0;
        virtual int getCategoryFlags() const   = 0;
        virtual std::string toString() const
        {
            return getName();
        }

        inline bool isInCategory(EventCategory category)
        {
            return getCategoryFlags() & (int)category;
        }

        bool Handled = false;
    };

    class EventDispatcher
    {
        template<typename T>
        using EventFn = std::function<bool(T &)>;

      public:
        EventDispatcher(Event &event): event(event) {}

        template<typename T>
        bool Dispatch(EventFn<T> func)
        {
            if(event.getEventType() == T::getStaticType())
            {
                event.Handled = func(*(T *)&event);
                return true;
            }

            return false;
        }

      private:
        Event &event;
    };

    inline std::ostream &operator<<(std::ostream &os, const Event &e)
    {
        return os << e.toString();
    }

} // namespace FRIGGA_NAMESPACE
