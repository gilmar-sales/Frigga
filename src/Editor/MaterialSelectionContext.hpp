#pragma once

#include <cstdint>

class MaterialSelectionContext
{
  public:
    static constexpr std::uint32_t Invalid = 0;

    [[nodiscard]] bool HasSelection() const
    {
        return mSelected != Invalid;
    }

    [[nodiscard]] std::uint32_t Get() const
    {
        return mSelected;
    }

    void Select(std::uint32_t materialId)
    {
        mSelected = materialId;
    }

    void Clear()
    {
        mSelected = Invalid;
    }

  private:
    std::uint32_t mSelected = Invalid;
};
