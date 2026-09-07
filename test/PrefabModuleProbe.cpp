#include <filesystem>
#include <string_view>

namespace fg
{
    class Scene;

    // Declarations must match Prefab.hpp so UND mangled names bind to the host.
    class Prefab
    {
      public:
        static bool Instantiate(Scene &scene, std::string_view json, unsigned parent,
                                unsigned &outRoot);
        static bool Load(Scene &scene, const std::filesystem::path &path, unsigned parent,
                         unsigned &outRoot);
    };
} // namespace fg

/// Minimal gameplay-module stand-in: leaves Prefab::* undefined so dlopen against
/// a host with --export-dynamic proves the symbols are resolvable.
extern "C" __attribute__((visibility("default"))) int fri_probe_prefab_api()
{
    volatile auto load        = &fg::Prefab::Load;
    volatile auto instantiate = &fg::Prefab::Instantiate;
    return (load != nullptr && instantiate != nullptr) ? 1 : 0;
}
