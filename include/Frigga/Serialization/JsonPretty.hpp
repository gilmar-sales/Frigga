#pragma once

#include <Frigga/Macro.hpp>

#include <string>
#include <string_view>

namespace FRIGGA_NAMESPACE
{
    /// Format minified JSON (e.g. simdjson::to_json output) with newlines and
    /// indentation so project files stay human-readable. Values, key order and
    /// string contents are preserved byte-for-byte; only whitespace outside of
    /// strings is added. Invalid JSON is returned unchanged.
    [[nodiscard]] std::string PrettifyJson(std::string_view minified, int indentStep = 2);

} // namespace FRIGGA_NAMESPACE
