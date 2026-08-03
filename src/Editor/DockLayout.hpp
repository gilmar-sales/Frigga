#pragma once

#include <string>

// Per-workflow dock identity. Window titles use ImGui's ### operator so the
// visible label stays the same while each layout keeps an independent dock id.
namespace EditorDock
{
    inline const char *g_layoutId = "Default";

    inline void SetLayoutId(const char *id)
    {
        g_layoutId = id ? id : "Default";
    }

    inline const char *GetLayoutId()
    {
        return g_layoutId;
    }

    inline std::string WindowId(const char *title)
    {
        return std::string(title) + "###" + g_layoutId + "_" + title;
    }
}
