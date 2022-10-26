#pragma once

#define FRIGGA_NAMESPACE fg
#define FRIGGA_BEGIN                                                                                                   \
    namespace FRIGGA_NAMESPACE                                                                                         \
    {
#define FRIGGA_END }

#define IMGUI_IMPL_OPENGL_LOADER_GLBINDING3
#define GLFW_INCLUDE_NONE

#ifdef _WIN32
#ifdef _WIN64
#define FG_PLATFORM_WINDOWS
#else
#error windows 32-bits not supported!
#endif
#elif defined(__APPLE__) || defined(__MACH__)
#define FG_PLATFORM_MACOS
#elif defined(__linux__)
#define FG_PLATFORM_LINUX
#endif

#ifndef NDEBUG
#define FG_DEBUG 1
#endif

#ifdef FG_DEBUG
#if defined(FG_PLATFORM_WINDOWS)
#define debug_break() __debugbreak()
#elif defined(FG_PLATFORM_LINUX)
#include <signal.h>
#define debug_break() raise(SIGTRAP)
#elif defined(FG_PLATFORM_MACOSX)
#define debug_break() __builtin_trap()
#else
#error "Platform doesn't support debugbreak yet!"
#endif
#define FG_ASSERT
#else
#define debug_break()
#endif

#define BIT(x) (1 << x)

#ifndef FG_DISABLE_ASSERTIONS
#define FG_ASSERT(expression)                                                                                          \
    if(!(expression))                                                                                                  \
    {                                                                                                                  \
        FG_LOG("core", "assertion", fmt::emphasis::bold | fmt::fg(fmt::color::red),                                    \
               "expression: " #expression " failed!");                                                                 \
        debug_break();                                                                                                 \
    }
#endif

using uint   = unsigned int;
using ulong  = unsigned long;
using ushort = unsigned short;

template<typename T>
using shared = std::shared_ptr<T>;

template<typename T>
using unique = std::unique_ptr<T>;