# Networking (ECS + ENet)

Camada de multiplayer autoritativa para a Frigga, desenhada para o modelo ECS
do Freyr e transportada sobre ENet (UDP confiável + não-confiável).

## Padrões adotados (estado da arte)

| Padrão | Onde se aplica | Referências |
|---|---|---|
| Servidor autoritativo | `NetworkServer` é a única fonte de verdade; clientes enviam *inputs*, nunca estado final | Counter-Strike, Valorant, Overwatch |
| Snapshot replication + interpolação | Servidor difunde `Snapshot` a 20 Hz no canal não-confiável; `NetworkClient` interpola com atraso de 100 ms | Source engine, Unity Netcode |
| Client prediction (exclusão de interpolação) | Entidades com `NetworkOwner{isLocal}` não são interpoladas no dono | Rocket League, Source |
| Canais separados | Canal 0 confiável (Spawn/Despawn/RPC), canal 1 não-confiável sequenciado (snapshots/inputs) | ENet, bevy_replicon |
| NetId estável | `fr::Entity` é reciclável e local à sessão; tudo no fio usa `NetworkIdentity::netId` (u32 monotônico) | Unity Netcode `NetworkObjectId`, SpatialOS |
| Controller vs. System | `NetworkController` só marca intenções/componentes; `NetworkServerSystem`/`NetworkClientSystem` detêm o backend | Mesma divisão de `AudioController`/`AudioSystem` |

Padrões deliberadamente **fora** do v1: rollback determinístico (GGPO —
exige simulação 100% determinística, incompatível com Jolt), compressão delta
e interest management (ganhos só aparecem acima de ~100 entidades/64 players;
a API já isola o ponto de extensão: `BroadcastSnapshot`).

## Peças

- `include/Frigga/Network/NetworkProtocol.hpp` — `NetWriter`/`NetReader`
  little-endian com checagem de limites, `EncodeSnapshot`/`DecodeSnapshot`,
  `EncodeSpawn`/`DecodeSpawn`, canais e constantes (`kNetReliableChannel`,
  `kNetStateChannel`, `kDefaultSnapshotHz = 20`, `kDefaultInterpolationDelay`).
- `include/Frigga/Network/NetworkComponents.hpp` — `NetworkIdentity`
  (netId, ownerClientId, prefab), `Replicated` (opt-in de snapshot),
  `NetworkOwner` (dono local, excluído da interpolação).
- `include/Frigga/Network/INetworkTransport.hpp` — interfaces
  `IServerTransport`/`IClientTransport` (injeção de fakes nos testes).
- `include/Frigga/Network/EnetTransport.hpp` — `EnetServerTransport`,
  `EnetClientTransport` e `EnetRuntime` (init/deinit com ref-count).
- `include/Frigga/Network/NetworkServer.hpp` — netId allocation, mapa
  netId↔entidade, broadcast de snapshots, filas de input/RPC, re-envio de
  Spawn para joiners.
- `include/Frigga/Network/NetworkClient.hpp` — jitter buffer, Spawn/Despawn
  aplicados inline, interpolação (lerp posição/escala + slerp rotação).
- `include/Frigga/Network/NetworkController.hpp` — API de gameplay:
  `Host`/`StopHost`, `Join`/`Leave`, `Replicate`/`Unreplicate`,
  `SendInput`/`SendRpc`.
- `include/Frigga/ECS/Systems/NetworkSystems.hpp` — `NetworkServerSystem` e
  `NetworkClientSystem` (pipeline `Simulation`).

## Uso

```cpp
// Servidor (host)
auto controller = services->GetService<fg::NetworkController>();
controller->Host(7777);
const std::uint32_t netId = controller->Replicate(entity); // adiciona NetworkIdentity + Replicated

// Cliente
controller->Join("127.0.0.1", 7777);

// Inputs do jogador (canal de estado, pode perder)
controller->SendInput(netId, bytes);

// RPCs (canal confiável)
controller->SendRpc(netId, rpcId, payload);

// Servidor consome inputs no sistema de gameplay (após NetworkServerSystem::Poll):
for(auto &input : server->ConsumeInputs()) { /* aplica ao tick autoritativo */ }
for(auto &rpc : server->ConsumeRpcs()) { /* ... */ }
```

## Fio (binário, little-endian)

- `Spawn`: `[u8=1][u32 netId][u32 owner][vec3 pos][quat rot][vec3 scale][u16 len + prefab]`
- `Despawn`: `[u8=2][u32 netId]`
- `Snapshot`: `[u8=3][u32 tick][u16 count]([u32 netId][vec3][quat][vec3])*`
- `Input`: `[u8=4][u32 tick][u32 netId][u16 len + bytes]`
- `Rpc`: `[u8=5][u32 netId][u32 rpcId][u16 len + bytes]`

## Testes

- `test/NetworkProtocolSpec.cpp` — round-trip do protocolo, rejeição de
  pacotes truncados/tipo errado.
- `test/NetworkReplicationSpec.cpp` — spawn/snapshot/input/despawn fim-a-fim
  sobre transporte fake em memória + init do ENet (`EnetRuntime`).
