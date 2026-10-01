#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace FRIGGA_NAMESPACE
{
    class CrashReporter
    {
      public:
        static void Install(std::filesystem::path reportPath);

        /// Human-readable "NAME — hint" for a signal number. Testable without crashing.
        static std::string_view DescribeSignal(int signalNumber);

        /// Attach key/value information to subsequent crash reports.
        static void SetContext(std::string key, std::string value);

        /// Record a recent event in the bounded crash-report breadcrumb trail.
        static void AddBreadcrumb(std::string_view message);
    };

    class FrameProfiler
    {
      public:
        explicit FrameProfiler(std::filesystem::path tracePath);
        ~FrameProfiler();

        void Record(std::string_view name, std::chrono::steady_clock::duration duration);

      private:
        struct Event
        {
            std::string name;
            std::int64_t timestampUs = 0;
            std::int64_t durationUs  = 0;
        };

        std::filesystem::path mTracePath;
        std::chrono::steady_clock::time_point mStarted;
        std::vector<Event> mEvents;
        std::mutex mMutex;
    };
} // namespace FRIGGA_NAMESPACE
