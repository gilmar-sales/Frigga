#pragma once

/**
 * Plugin translation-unit prelude (injected via target_precompile_headers).
 * `fg` / `fra` must exist before Freya Event.hpp and Frigga public headers.
 */
#include <Frigga/Macro.hpp>

#ifndef FREYA_NAMESPACE
#    define FREYA_NAMESPACE fra
#endif
