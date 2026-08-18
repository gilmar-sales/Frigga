#pragma once

#include <Frigga/Scene/Scene.hpp>
#include <Frigga/Scene/SceneSerializer.hpp>

#include <Freyr/Freyr.hpp>

#include <optional>
#include <string>
#include <string_view>

class ComponentClipboard
{
  public:
    static constexpr const char *kMagicPrefix = "frigga/component/v1\n";

    static bool Copy(const fg::Scene &scene, fr::Entity entity, std::string_view kind);
    static bool Paste(fg::Scene &scene, fr::Entity entity);
    [[nodiscard]] static bool HasData();
    [[nodiscard]] static std::string_view GetKind();

  private:
    [[nodiscard]] static std::optional<std::string_view> ReadClipboardPayload();

    static inline std::string sJson;
    static inline std::string sKind;
};
