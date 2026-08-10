#pragma once

#include <Freyr/Freyr.hpp>

#include <Frigga/Macro.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

namespace FRIGGA_NAMESPACE
{

    enum class PropertyKind : std::uint8_t
    {
        Bool = 0,
        Int64,
        Float,
        String,
        Vec2,
        Vec3,
        Vec4,
    };

    struct PropertyValue
    {
        PropertyKind kind = PropertyKind::Float;
        bool         boolValue   = false;
        std::int64_t intValue    = 0;
        float        floatValue  = 0.0f;
        std::string  stringValue {};
        glm::vec2    vec2Value {0.0f};
        glm::vec3    vec3Value {0.0f};
        glm::vec4    vec4Value {0.0f};
    };

    struct NamedProperty
    {
        std::string   name;
        PropertyValue value;
    };

    /// DTO for scene JSON / inspector edit bags (not a Freyr component).
    struct UserComponentInstance
    {
        std::string                typeId;
        std::vector<NamedProperty> properties;
    };

    [[nodiscard]] inline NamedProperty *FindProperty(UserComponentInstance &instance,
                                                     std::string_view name)
    {
        for(auto &property : instance.properties)
        {
            if(property.name == name)
            {
                return &property;
            }
        }
        return nullptr;
    }

    [[nodiscard]] inline const NamedProperty *FindProperty(const UserComponentInstance &instance,
                                                           std::string_view name)
    {
        for(const auto &property : instance.properties)
        {
            if(property.name == name)
            {
                return &property;
            }
        }
        return nullptr;
    }

} // namespace FRIGGA_NAMESPACE
