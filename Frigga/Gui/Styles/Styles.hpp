#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

#include <glbinding/gl/gl.h>
#include <glbinding/gl45core/gl.h>
#include <glbinding/gl45ext/gl.h>
#include <glbinding/glbinding.h>
#include <GLFW/glfw3.h>

FRIGGA_BEGIN

void StylePhantomLight(ImGuiStyle* dst = nullptr);
void StylePhantomDark(ImGuiStyle* dst = nullptr);

FRIGGA_END
