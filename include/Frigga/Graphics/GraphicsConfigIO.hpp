#pragma once

#include "Frigga/Graphics/GraphicsConfig.hpp"

#include <Freya/Builders/FreyaOptionsBuilder.hpp>

#include <filesystem>
#include <string>
#include <string_view>

namespace FRIGGA_NAMESPACE
{

    [[nodiscard]] std::string SerializeGraphicsConfig(const GraphicsConfig &config);
    [[nodiscard]] bool ParseGraphicsConfig(std::string_view json, GraphicsConfig &out,
                                           std::string *error = nullptr);
    [[nodiscard]] bool LoadGraphicsConfigFile(const std::filesystem::path &path,
                                              GraphicsConfig &out, std::string *error = nullptr);
    [[nodiscard]] bool SaveGraphicsConfigFile(const std::filesystem::path &path,
                                              const GraphicsConfig &config,
                                              std::string *error = nullptr);

    /// Writes @p config when the file is missing; never overwrites an existing file.
    [[nodiscard]] bool EnsureGraphicsConfigFile(const std::filesystem::path &path,
                                                const GraphicsConfig &config,
                                                std::string *error = nullptr);

    /// Maps Freya quality ordinals (0=Low … 3=Ultra, 4=Off) to JSON labels.
    [[nodiscard]] std::string_view GraphicsQualityLabel(int quality);

    /// Applies project graphics settings to Freya before the device/window are built.
    void ApplyGraphicsConfig(fra::FreyaOptionsBuilder &builder, const GraphicsConfig &config);

} // namespace FRIGGA_NAMESPACE
