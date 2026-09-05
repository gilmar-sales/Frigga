#include <Frigga/Diagnostics/RuntimeDiagnostics.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

TEST(RuntimeDiagnostics, WritesChromeTrace)
{
    const auto path = std::filesystem::temp_directory_path() / "frigga-test-trace.json";
    {
        fg::FrameProfiler profiler(path);
        profiler.Record("test-frame", std::chrono::milliseconds(1));
    }

    std::ifstream file(path);
    const std::string trace((std::istreambuf_iterator<char>(file)), {});
    EXPECT_NE(trace.find("\"traceEvents\""), std::string::npos);
    EXPECT_NE(trace.find("test-frame"), std::string::npos);
    std::error_code ec;
    std::filesystem::remove(path, ec);
}
