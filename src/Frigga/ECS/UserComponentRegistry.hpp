#pragma once

#include "Frigga/ECS/Components/UserDataComponent.hpp"

#include <Freyr/Freyr.hpp>

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

    /**
     * Type-erased ops for a plugin Freyr component (instantiated in the .so TU).
     * Storage is the real SoA component; the Editor only holds these callbacks.
     */
    struct RuntimeComponentOps
    {
        std::string                         typeId;
        std::string                         displayName;
        fr::ComponentId                     componentId = 0;
        std::vector<UserComponentFieldDesc> fields;
        UserComponentInstance               defaultInstance {};

        std::function<void(fr::Registry &, fr::Entity)> addDefault;
        std::function<void(fr::Registry &, fr::Entity)> remove;
        std::function<bool(fr::Registry &, fr::Entity)> has;
        std::function<bool(fr::Registry &, fr::Entity, UserComponentInstance &)> toInstance;
        std::function<void(fr::Registry &, fr::Entity, const UserComponentInstance &)> fromInstance;
        std::function<void(fr::Registry &)> removeFromAllEntities;
        std::function<bool(fr::Registry &)> unregisterFromFreyr;
    };

    /**
     * Runtime catalogue of gameplay/plugin component types.
     * Cleared when the gameplay plugin detaches (after strip + Freyr unregister).
     */
    class UserComponentRegistry
    {
      public:
        void Register(RuntimeComponentOps ops);
        /// Strip entities + Freyr unregister via ops, then clear catalogue. Call before dlclose.
        void DetachAll(fr::Registry &registry);
        void ClearPluginTypes();

        [[nodiscard]] bool Has(std::string_view typeId) const;
        [[nodiscard]] std::optional<RuntimeComponentOps> Find(std::string_view typeId) const;
        [[nodiscard]] std::vector<RuntimeComponentOps> GetTypes() const;

      private:
        mutable std::mutex mMutex;
        std::unordered_map<std::string, RuntimeComponentOps> mTypes;
        std::vector<std::string> mOrder;
    };

} // namespace FRIGGA_NAMESPACE
