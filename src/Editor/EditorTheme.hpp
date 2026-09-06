#pragma once

enum class EditorTheme
{
    PhantomDark  = 0,
    PhantomLight = 1,
    Dark         = 2,
    Light        = 3,
    Classic      = 4,
    Default      = PhantomDark
};

void ApplyTheme(EditorTheme theme);
