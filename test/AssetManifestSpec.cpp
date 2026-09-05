#include <Frigga/Asset/AssetCooker.hpp>
#include <Frigga/Asset/AssetManifest.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>

TEST(AssetManifest, PersistsStableIdsAcrossReload)
{
    const auto root = std::filesystem::temp_directory_path() /
                      ("frigga-assets-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);

    fg::AssetManifest first;
    ASSERT_TRUE(first.Load(root));
    const auto modelId = first.GetOrCreate("Models/ship.glb", "model");
    ASSERT_FALSE(modelId.empty());
    ASSERT_TRUE(first.Save(root));

    fg::AssetManifest second;
    ASSERT_TRUE(second.Load(root));
    EXPECT_EQ(second.GetOrCreate("Models/ship.glb", "model"), modelId);
    EXPECT_NE(second.GetOrCreate("Models/ship.glb", "texture"), modelId);

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

TEST(AssetManifest, RejectsMalformedManifest)
{
    const auto root = std::filesystem::temp_directory_path() /
                      ("frigga-assets-invalid-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    {
        std::ofstream file(root / fg::AssetManifest::FileName);
        file << "{\"version\":1,\"assets\":[";
    }

    fg::AssetManifest manifest;
    std::string error;
    EXPECT_FALSE(manifest.Load(root, &error));
    EXPECT_NE(error.find("Invalid asset manifest JSON"), std::string::npos);

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

TEST(AssetManifest, DetectsChangedAndOrphanedResources)
{
    const auto root = std::filesystem::temp_directory_path() /
                      ("frigga-assets-scan-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root / "Models");
    {
        std::ofstream file(root / "Models" / "ship.glb");
        file << "original";
    }

    fg::AssetManifest manifest;
    ASSERT_TRUE(manifest.Load(root));
    manifest.RecordImport("Models/ship.glb", "model", root / "Models/ship.glb");
    ASSERT_TRUE(manifest.Save(root));

    {
        std::ofstream file(root / "Models" / "ship.glb", std::ios::app);
        file << "changed";
    }
    {
        std::ofstream file(root / "Models" / "orphan.glb");
        file << "orphan";
    }

    const auto validation = manifest.Validate(root);
    EXPECT_EQ(validation.missing.size(), 0u);
    EXPECT_EQ(validation.changed.size(), 1u);
    ASSERT_EQ(validation.orphaned.size(), 1u);
    EXPECT_EQ(validation.orphaned.front(), "Models/orphan.glb");

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

TEST(AssetCooker, CopiesResourcesAndManifestDeterministically)
{
    const auto root = std::filesystem::temp_directory_path() /
                      ("frigga-assets-cook-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto cooked = root / "cooked";
    const auto resources = root / "Resources";
    std::filesystem::create_directories(resources / "Textures");
    {
        std::ofstream file(resources / "Textures" / "gray.txt");
        file << "gray";
    }

    fg::AssetManifest manifest;
    ASSERT_TRUE(manifest.Load(resources));
    manifest.RecordImport("Textures/gray.txt", "texture", resources / "Textures" / "gray.txt");
    ASSERT_TRUE(manifest.Save(resources));

    const auto result = fg::AssetCooker::Cook(resources, cooked);
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_TRUE(std::filesystem::exists(cooked / "Textures/gray.txt"));
    EXPECT_TRUE(std::filesystem::exists(cooked / fg::AssetManifest::FileName));

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}
