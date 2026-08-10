#pragma once

#include "Frigga/ECS/Components/UserDataComponent.hpp"

#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace FRIGGA_NAMESPACE
{

    struct UserComponentFieldDesc
    {
        std::string  name;
        PropertyKind kind = PropertyKind::Float;
    };

    struct UserComponentTypeDesc
    {
        std::string                         typeId;
        std::string                         displayName;
        std::vector<UserComponentFieldDesc> fields;
        /// Frozen at Register time (plain data — safe for the Editor to copy).
        UserComponentInstance               defaultInstance {};
        std::function<UserComponentInstance()> makeDefault;
    };

    /**
     * Runtime catalogue of gameplay/plugin component types registered via C++26 reflection.
     * Cleared when the gameplay plugin detaches.
     */
    class UserComponentRegistry
    {
      public:
        void Register(UserComponentTypeDesc desc);
        void ClearPluginTypes();

        [[nodiscard]] bool Has(std::string_view typeId) const;
        [[nodiscard]] std::optional<UserComponentTypeDesc> Find(std::string_view typeId) const;
        [[nodiscard]] std::vector<UserComponentTypeDesc> GetTypes() const;

      private:
        mutable std::mutex mMutex;
        std::unordered_map<std::string, UserComponentTypeDesc> mTypes;
        std::vector<std::string> mOrder;
    };

} // namespace FRIGGA_NAMESPACE
