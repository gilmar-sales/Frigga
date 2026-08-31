#pragma once

#include <Freyr/Freyr.hpp>

class SelectionContext
{
  public:
    static constexpr fr::Entity Invalid = static_cast<fr::Entity>(-1);

    [[nodiscard]] bool HasSelection() const
    {
        return mSelected != Invalid;
    }

    [[nodiscard]] fr::Entity Get() const
    {
        return mSelected;
    }

    void Select(fr::Entity entity)
    {
        mSelected = entity;
    }

    void Clear()
    {
        mSelected = Invalid;
    }

  private:
    fr::Entity mSelected = Invalid;
};
