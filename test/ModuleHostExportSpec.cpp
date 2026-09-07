#include <Frigga/Scene/Prefab.hpp>
#include <Frigga/Scene/SceneSerializer.hpp>

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>

#if !defined(_WIN32)
#    include <dlfcn.h>
#endif

#ifndef FRIGGA_PREFAB_MODULE_PROBE
#    define FRIGGA_PREFAB_MODULE_PROBE ""
#endif

namespace
{
    [[nodiscard]] bool RuntimeDynsymContains(const char *needle)
    {
        const auto runtime = std::filesystem::current_path() / "Runtime";
        if(!std::filesystem::is_regular_file(runtime))
        {
            return false;
        }

        const auto cmd =
            "nm -D --defined-only \"" + runtime.string() + "\" 2>/dev/null | c++filt";
        FILE *pipe = popen(cmd.c_str(), "r");
        if(!pipe)
        {
            return false;
        }

        bool found = false;
        char buffer[512];
        while(fgets(buffer, sizeof(buffer), pipe))
        {
            if(std::string_view(buffer).find(needle) != std::string_view::npos)
            {
                found = true;
                break;
            }
        }
        pclose(pipe);
        return found;
    }
} // namespace

TEST(ModuleHostExport, PrefabKeepAlivePullsPublicApi)
{
    fg::FriKeepPrefabSymbols();
    EXPECT_NE(&fg::Prefab::Load, nullptr);
    EXPECT_NE(&fg::Prefab::Instantiate, nullptr);
    EXPECT_NE(&fg::SceneSerializer::InstantiatePrefab, nullptr);
}

#if !defined(_WIN32) && !defined(__APPLE__)
TEST(ModuleHostExport, RuntimeExportsPrefabForGameplayModules)
{
    ASSERT_TRUE(std::filesystem::is_regular_file(std::filesystem::current_path() / "Runtime"));
    EXPECT_TRUE(RuntimeDynsymContains("fg::Prefab::Load"))
        << "Runtime must export Prefab::Load for module dlopen";
    EXPECT_TRUE(RuntimeDynsymContains("fg::Prefab::Instantiate"))
        << "Runtime must export Prefab::Instantiate for module dlopen";
}

TEST(ModuleHostExport, DlopenProbeResolvesPrefabAgainstHost)
{
    fg::FriKeepPrefabSymbols();

    const char *probePath = FRIGGA_PREFAB_MODULE_PROBE;
    ASSERT_NE(probePath[0], '\0') << "FRIGGA_PREFAB_MODULE_PROBE not configured";
    ASSERT_TRUE(std::filesystem::is_regular_file(probePath)) << probePath;

    dlerror();
    void *handle = dlopen(probePath, RTLD_NOW | RTLD_LOCAL);
    ASSERT_NE(handle, nullptr) << dlerror();

    using ProbeFn = int (*)();
    auto *probe   = reinterpret_cast<ProbeFn>(dlsym(handle, "fri_probe_prefab_api"));
    ASSERT_NE(probe, nullptr) << dlerror();
    EXPECT_EQ(probe(), 1);
    dlclose(handle);
}
#endif
