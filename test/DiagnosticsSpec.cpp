#include <Frigga/Diagnostics/RuntimeDiagnostics.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <csignal>
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

TEST(RuntimeDiagnostics, DescribesCrashSignals)
{
    EXPECT_NE(std::string(fg::CrashReporter::DescribeSignal(SIGFPE)).find("SIGFPE"),
              std::string::npos);
    EXPECT_NE(std::string(fg::CrashReporter::DescribeSignal(SIGSEGV)).find("SIGSEGV"),
              std::string::npos);
    EXPECT_NE(std::string(fg::CrashReporter::DescribeSignal(SIGABRT)).find("SIGABRT"),
              std::string::npos);
    EXPECT_NE(std::string(fg::CrashReporter::DescribeSignal(SIGILL)).find("SIGILL"),
              std::string::npos);
}

TEST(RuntimeDiagnostics, AcceptsCrashContextAndBreadcrumbs)
{
    EXPECT_NO_THROW(fg::CrashReporter::SetContext("test_key", "test_value"));
    EXPECT_NO_THROW(fg::CrashReporter::AddBreadcrumb("test breadcrumb"));
}
