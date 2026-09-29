#pragma once

#include <Frigga/Macro.hpp>

#include <Freyr/Freyr.hpp>

#include <cstdint>
#include <string>

namespace FRIGGA_NAMESPACE
{

    inline constexpr std::uint32_t kInvalidNetId   = 0;
    inline constexpr std::uint32_t kServerClientId = 0;

    /// Stable network identity. Freyr `fr::Entity` ids are session-local and
    /// recycled, so replication maps everything through this netId instead.
    struct NetworkIdentity: fr::Component
    {
        std::uint32_t netId = kInvalidNetId;
        /// Owning client (kServerClientId = server-owned). Owners may send
        /// inputs for this entity; the server stays authoritative.
        std::uint32_t ownerClientId = kServerClientId;
        bool serverAuthoritative    = true;
        /// Optional prefab path used by Spawn replication. Empty = bare entity.
        std::string prefab {};
    };

    /// Opt-in replication tag. The snapshot set is exactly
    /// { entities with NetworkIdentity + Replicated + TransformComponent }.
    /// RegisterEntity / NetworkController::Replicate add both components.
    struct Replicated: fr::Component
    {
        /// Last server tick this entity was included in a snapshot.
        std::uint32_t lastSentTick = 0;
    };

    /// Marks the locally-controlled player entity on clients (input source)
    /// and on the server (which client owns it).
    struct NetworkOwner: fr::Component
    {
        std::uint32_t clientId = kInvalidNetId;
        bool isLocal           = false;
    };

} // namespace FRIGGA_NAMESPACE
