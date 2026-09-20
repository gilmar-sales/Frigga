#pragma once

#include "Frigga/Macro.hpp"

namespace FRIGGA_NAMESPACE
{
    /// Keep Freya screen-UI entry points linked into Editor/Runtime so Windows
    /// module exports (and Linux --export-dynamic) can resolve gameplay HUD calls.
    void FriKeepFreyaUiSymbols();
} // namespace FRIGGA_NAMESPACE
