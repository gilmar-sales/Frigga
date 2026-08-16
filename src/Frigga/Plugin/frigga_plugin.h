#pragma once

/**
 * @file frigga_plugin.h
 * @brief Stable C ABI for Frigga gameplay plugins (shared libraries).
 *
 * Plugins are built against the same Freyr/Frigga tree as the Editor and cast
 * FriHost::registry to fr::Registry*. Gameplay/plugin component types are
 * registered into FriHost::user_components (UserComponentRegistry*) via
 * C++26 reflection helpers in UserComponentReflection.hpp.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#    ifdef FRI_PLUGIN_EXPORTS
#        define FRI_PLUGIN_API __declspec(dllexport)
#    else
#        define FRI_PLUGIN_API __declspec(dllimport)
#    endif
#else
#    define FRI_PLUGIN_API __attribute__((visibility("default")))
#endif

typedef struct FriHost
{
    /** Opaque pointer to fr::Registry in the Editor process. */
    void *registry;
    /** Opaque pointer to fg::UserComponentRegistry in the Editor process. */
    void *user_components;
    /** Opaque pointer to fr::SystemManager in the Editor process. */
    void *system_manager;
    /** Opaque pointer to skr::ServiceProvider (host) for late AddSingleton / Remove. */
    void *services;
    /** Plugin id from the project (e.g. "gameplay"). May be NULL. */
    const char *plugin_id;
    /** Menu label (plugin.json name). May be NULL; fall back to plugin_id. */
    const char *plugin_name;
} FriHost;

typedef struct FriPlugin FriPlugin;

typedef struct FriPluginApi
{
    FriPlugin *(*create)(void);
    void (*destroy)(FriPlugin *plugin);
    void (*on_attach)(FriPlugin *plugin, const FriHost *host);
    void (*on_detach)(FriPlugin *plugin);
    void (*on_update)(FriPlugin *plugin, float delta_time);
} FriPluginApi;

/** Exported entry point resolved via dlsym / GetProcAddress. */
FRI_PLUGIN_API const FriPluginApi *fri_plugin_api(void);

#ifdef __cplusplus
}
#endif
