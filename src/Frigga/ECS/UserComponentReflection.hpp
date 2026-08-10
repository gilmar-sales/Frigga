#pragma once

#include "Frigga/ECS/Components/UserDataComponent.hpp"
#include "Frigga/ECS/UserComponentRegistry.hpp"

#include <glm/glm.hpp>

#include <concepts>
#include <cstdint>
#include <meta>
#include <string>
#include <string_view>
#include <type_traits>

namespace FRIGGA_NAMESPACE
{
    namespace user_comp_detail
    {
        template <typename T>
        concept IsBool = std::same_as<std::remove_cvref_t<T>, bool>;

        template <typename T>
        concept IsInteger = std::integral<std::remove_cvref_t<T>> && !IsBool<T>;

        template <typename T>
        concept IsFloat = std::floating_point<std::remove_cvref_t<T>>;

        template <typename T>
        concept IsString = std::same_as<std::remove_cvref_t<T>, std::string>;

        template <typename T>
        concept IsVec2 = std::same_as<std::remove_cvref_t<T>, glm::vec2>;

        template <typename T>
        concept IsVec3 = std::same_as<std::remove_cvref_t<T>, glm::vec3>;

        template <typename T>
        concept IsVec4 = std::same_as<std::remove_cvref_t<T>, glm::vec4>;

        template <typename T>
        concept IsSupportedField =
            IsBool<T> || IsInteger<T> || IsFloat<T> || IsString<T> || IsVec2<T> || IsVec3<T> ||
            IsVec4<T>;

        template <typename T>
            requires IsSupportedField<T>
        PropertyKind KindOf()
        {
            using U = std::remove_cvref_t<T>;
            if constexpr(IsBool<U>)
            {
                return PropertyKind::Bool;
            }
            else if constexpr(IsInteger<U>)
            {
                return PropertyKind::Int64;
            }
            else if constexpr(IsFloat<U>)
            {
                return PropertyKind::Float;
            }
            else if constexpr(IsString<U>)
            {
                return PropertyKind::String;
            }
            else if constexpr(IsVec2<U>)
            {
                return PropertyKind::Vec2;
            }
            else if constexpr(IsVec3<U>)
            {
                return PropertyKind::Vec3;
            }
            else
            {
                return PropertyKind::Vec4;
            }
        }

        template <typename T>
            requires IsSupportedField<T>
        PropertyValue ToValue(const T &field)
        {
            using U = std::remove_cvref_t<T>;
            PropertyValue value {};
            value.kind = KindOf<U>();
            if constexpr(IsBool<U>)
            {
                value.boolValue = field;
            }
            else if constexpr(IsInteger<U>)
            {
                value.intValue = static_cast<std::int64_t>(field);
            }
            else if constexpr(IsFloat<U>)
            {
                value.floatValue = static_cast<float>(field);
            }
            else if constexpr(IsString<U>)
            {
                value.stringValue = field;
            }
            else if constexpr(IsVec2<U>)
            {
                value.vec2Value = field;
            }
            else if constexpr(IsVec3<U>)
            {
                value.vec3Value = field;
            }
            else if constexpr(IsVec4<U>)
            {
                value.vec4Value = field;
            }
            return value;
        }

        template <typename T>
            requires IsSupportedField<T>
        void FromValue(T &field, const PropertyValue &value)
        {
            using U = std::remove_cvref_t<T>;
            if constexpr(IsBool<U>)
            {
                if(value.kind == PropertyKind::Bool)
                {
                    field = value.boolValue;
                }
            }
            else if constexpr(IsInteger<U>)
            {
                if(value.kind == PropertyKind::Int64)
                {
                    field = static_cast<U>(value.intValue);
                }
                else if(value.kind == PropertyKind::Float)
                {
                    field = static_cast<U>(value.floatValue);
                }
            }
            else if constexpr(IsFloat<U>)
            {
                if(value.kind == PropertyKind::Float)
                {
                    field = static_cast<U>(value.floatValue);
                }
                else if(value.kind == PropertyKind::Int64)
                {
                    field = static_cast<U>(value.intValue);
                }
            }
            else if constexpr(IsString<U>)
            {
                if(value.kind == PropertyKind::String)
                {
                    field = value.stringValue;
                }
            }
            else if constexpr(IsVec2<U>)
            {
                if(value.kind == PropertyKind::Vec2)
                {
                    field = value.vec2Value;
                }
            }
            else if constexpr(IsVec3<U>)
            {
                if(value.kind == PropertyKind::Vec3)
                {
                    field = value.vec3Value;
                }
            }
            else if constexpr(IsVec4<U>)
            {
                if(value.kind == PropertyKind::Vec4)
                {
                    field = value.vec4Value;
                }
            }
        }

        /// GCC reflection: avoid refl::for_each_member (stores vector<meta::info> as
        /// constexpr), which fails consteval allocation checks. Use define_static_array.
        template <typename T, typename Fn>
        constexpr void ForEachMember(T &obj, Fn &&fn)
        {
            template for(constexpr auto member : std::define_static_array(
                             std::meta::nonstatic_data_members_of(
                                 ^^T, std::meta::access_context::current())))
            {
                fn(std::meta::display_string_of(member), obj.[:member:]);
            }
        }
    } // namespace user_comp_detail

    template <typename T>
    UserComponentInstance ToUserComponentInstance(const T &object, std::string_view typeId)
    {
        UserComponentInstance instance;
        instance.typeId = std::string(typeId);
        T mutableCopy   = object;
        user_comp_detail::ForEachMember(mutableCopy, [&](std::string_view name, auto &field) {
            using FieldT = std::remove_cvref_t<decltype(field)>;
            if constexpr(user_comp_detail::IsSupportedField<FieldT>)
            {
                instance.properties.push_back(
                    NamedProperty {.name  = std::string(name),
                                   .value = user_comp_detail::ToValue(field)});
            }
        });
        return instance;
    }

    template <typename T>
    bool FromUserComponentInstance(const UserComponentInstance &instance, T &out)
    {
        bool any = false;
        user_comp_detail::ForEachMember(out, [&](std::string_view name, auto &field) {
            using FieldT = std::remove_cvref_t<decltype(field)>;
            if constexpr(user_comp_detail::IsSupportedField<FieldT>)
            {
                if(const auto *property = FindProperty(instance, name))
                {
                    user_comp_detail::FromValue(field, property->value);
                    any = true;
                }
            }
        });
        return any;
    }

    template <typename T>
    UserComponentTypeDesc BuildUserComponentTypeDesc(std::string_view typeId,
                                                     std::string_view displayName = {})
    {
        UserComponentTypeDesc desc;
        desc.typeId      = std::string(typeId);
        desc.displayName = displayName.empty() ? desc.typeId : std::string(displayName);

        T sample {};
        user_comp_detail::ForEachMember(sample, [&](std::string_view name, auto &field) {
            using FieldT = std::remove_cvref_t<decltype(field)>;
            if constexpr(user_comp_detail::IsSupportedField<FieldT>)
            {
                desc.fields.push_back(UserComponentFieldDesc {
                    .name = std::string(name),
                    .kind = user_comp_detail::KindOf<FieldT>(),
                });
            }
        });

        const std::string id = desc.typeId;
        desc.defaultInstance = ToUserComponentInstance(sample, id);
        desc.makeDefault     = [defaults = desc.defaultInstance]() { return defaults; };
        return desc;
    }

    template <typename T>
    void FriRegisterUserComponent(UserComponentRegistry &registry, std::string_view typeId,
                                  std::string_view displayName = {})
    {
        registry.Register(BuildUserComponentTypeDesc<T>(typeId, displayName));
    }

    template <typename T>
    bool FriTryGet(const UserDataComponent &data, std::string_view typeId, T &out)
    {
        const auto *instance = FindUserComponent(data, typeId);
        if(!instance)
        {
            return false;
        }
        out = T {};
        return FromUserComponentInstance(*instance, out);
    }

    template <typename T>
    void FriSet(UserDataComponent &data, std::string_view typeId, const T &value)
    {
        auto *instance = FindUserComponent(data, typeId);
        if(instance)
        {
            *instance = ToUserComponentInstance(value, typeId);
            return;
        }
        data.instances.push_back(ToUserComponentInstance(value, typeId));
    }

} // namespace FRIGGA_NAMESPACE
