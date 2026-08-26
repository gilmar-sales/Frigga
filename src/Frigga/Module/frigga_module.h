#pragma once

/**
 * @file frigga_module.h
 * @brief Stable C ABI for Frigga gameplay modules (shared libraries).
 *
 * Modules are built against the same Freyr/Frigga tree as the Editor and cast
 * FriHost::registry to fr::Registry*. Gameplay/module component types are
 * registered into FriHost::user_components (UserComponentRegistry*) via
 * C++26 reflection helpers in UserComponentReflection.hpp.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#    ifdef FRI_MODULE_EXPORTS
#        define FRI_MODULE_API __declspec(dllexport)
#    else
#        define FRI_MODULE_API __declspec(dllimport)
#    endif
#else
#    define FRI_MODULE_API __attribute__((visibility("default")))
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
    /** Module id from the project (e.g. "gameplay"). May be NULL. */
    const char *module_id;
    /** Menu label (module.json name). May be NULL; fall back to module_id. */
    const char *module_name;
} FriHost;

typedef struct FriModule FriModule;

typedef struct FriModuleApi
{
    FriModule *(*create)(void);
    void (*destroy)(FriModule *module);
    void (*on_attach)(FriModule *module, const FriHost *host);
    void (*on_detach)(FriModule *module);
    void (*on_update)(FriModule *module, float delta_time);
} FriModuleApi;

/** Exported entry point resolved via dlsym / GetProcAddress. */
FRI_MODULE_API const FriModuleApi *fri_module_api(void);

#ifdef __cplusplus
}
#endif
