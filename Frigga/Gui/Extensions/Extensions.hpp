#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

FRIGGA_BEGIN

void ToggleButton(const char *str_id = nullptr, bool *v = nullptr);

FRIGGA_END
