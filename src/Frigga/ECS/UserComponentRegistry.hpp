#pragma once

#include "Frigga/ECS/Components/UserDataComponent.hpp"
#include "Frigga/Module/FriComponentInspector.hpp"

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
        std::string                         moduleId;
        std::string                         moduleName;
        fr::ComponentId                     componentId = 0;
        std::vector<UserComponentFieldDesc> fields;
        UserComponentInstance               defaultInstance {};

        std::function<void(fr::Registry &, fr::Entity)> addDefault;
        std::function<void(fr::Registry &, fr::Entity)> remove;
        std::function<bool(fr::Registry &, fr::Entity)> has;
        std::function<bool(fr::Registry &, fr::Entity, UserComponentInstance &)> toInstance;
        std::function<void(fr::Registry &, fr::Entity, const UserComponentInstance &)> fromInstance;
        /// Optional custom inspector from plugin.Component<T>(..., Draw). Empty = reflection UI.
        std::function<void(fr::Registry &, fr::Entity, FriComponentInspector &)> drawInspector;
        /// Visit every entity that currently has this component (read-only iteration).
        std::function<void(fr::Registry &, const std::function<void(fr::Entity)> &)> forEachEntity;
        std::function<void(fr::Registry &)> removeFromAllEntities;
        std::function<bool(fr::Registry &)> unregisterFromFreyr;
    };

    struct UserComponentSnapshotEntry
    {
        fr::Entity              entity = 0;
        UserComponentInstance   instance {};
    };

    /// Type-erased bag used to keep gameplay components alive across plugin unload/reload.
    struct UserComponentWorldSnapshot
    {
        std::vector<UserComponentSnapshotEntry> entries;
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
        void ClearModuleTypes();

        /// Serialize every attached user-component instance (call before DetachAll).
        [[nodiscard]] UserComponentWorldSnapshot CaptureAll(fr::Registry &registry) const;
        /// Re-apply a snapshot after types are registered again. Skips unknown typeIds.
        /// @return number of instances successfully restored
        std::size_t RestoreAll(fr::Registry &registry, const UserComponentWorldSnapshot &snapshot);

        /// Stash a gameplay component read from disk while its typeId is not yet registered
        /// (plugin not loaded). Survives until ApplyDeferred or ClearDeferred; not cleared by
        /// DetachAll / ClearModuleTypes so first-build reload can still apply scene data.
        void EnqueueDeferred(fr::Entity entity, UserComponentInstance instance);
        void ClearDeferred();
        /// Apply every deferred instance whose typeId is now registered. Removes applied entries.
        /// @return number of instances successfully applied
        std::size_t ApplyDeferred(fr::Registry &registry);
        [[nodiscard]] std::vector<UserComponentSnapshotEntry> GetDeferred() const;
        [[nodiscard]] std::vector<UserComponentSnapshotEntry> GetDeferredForEntity(
            fr::Entity entity) const;

        [[nodiscard]] bool Has(std::string_view typeId) const;
        [[nodiscard]] std::optional<RuntimeComponentOps> Find(std::string_view typeId) const;
        [[nodiscard]] std::vector<RuntimeComponentOps> GetTypes() const;

      private:
        mutable std::mutex mMutex;
        std::unordered_map<std::string, RuntimeComponentOps> mTypes;
        std::vector<std::string> mOrder;
        std::vector<UserComponentSnapshotEntry> mDeferred;
    };

} // namespace FRIGGA_NAMESPACE
