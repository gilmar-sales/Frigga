#include <Frigga/Asset/AssetManifest.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main()
{
    const auto root = std::filesystem::temp_directory_path() / "frigga-benchmark-resources";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root, ec);
    std::ofstream(root / "asset.bin") << std::string(1024, 'x');

    fg::AssetManifest manifest;
    const auto start = std::chrono::steady_clock::now();
    constexpr int iterations = 1000;
    for(int i = 0; i < iterations; ++i)
    {
        manifest.RecordImport("asset.bin", "binary", root / "asset.bin");
        manifest.Save(root);
    }
    const auto elapsed = std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now() - start)
                             .count();
    std::cout << "AssetManifest.RecordImport+Save: " << iterations << " iterations, " << elapsed
              << " ms (" << elapsed / iterations << " ms/iteration)\n";
    std::filesystem::remove_all(root, ec);
    return 0;
}
