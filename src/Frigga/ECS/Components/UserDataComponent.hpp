#pragma once

#include <Frigga/Macro.hpp>

#include <Freyr/Freyr.hpp>

#include <cstdint>
#include <string>
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

    struct UserComponentInstance
    {
        std::string                typeId;
        std::vector<NamedProperty> properties;
    };

    /// Engine Freyr component that stores gameplay/plugin component instances as property bags.
    struct UserDataComponent: fr::Component
    {
        std::vector<UserComponentInstance> instances;
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

    [[nodiscard]] inline UserComponentInstance *FindUserComponent(UserDataComponent &data,
                                                                  std::string_view typeId)
    {
        for(auto &instance : data.instances)
        {
            if(instance.typeId == typeId)
            {
                return &instance;
            }
        }
        return nullptr;
    }

    [[nodiscard]] inline const UserComponentInstance *FindUserComponent(
        const UserDataComponent &data, std::string_view typeId)
    {
        for(const auto &instance : data.instances)
        {
            if(instance.typeId == typeId)
            {
                return &instance;
            }
        }
        return nullptr;
    }

} // namespace FRIGGA_NAMESPACE
