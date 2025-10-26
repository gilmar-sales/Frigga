#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

namespace FRIGGA_NAMESPACE {

void ToggleButton(const char *str_id = nullptr, bool *v = nullptr);

}
