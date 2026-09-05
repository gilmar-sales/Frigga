#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace FRIGGA_NAMESPACE
{
    class CrashReporter
    {
      public:
        static void Install(std::filesystem::path reportPath);
    };

    class FrameProfiler
    {
      public:
        explicit FrameProfiler(std::filesystem::path tracePath);
        ~FrameProfiler();

        void Record(std::string name, std::chrono::steady_clock::duration duration);

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
