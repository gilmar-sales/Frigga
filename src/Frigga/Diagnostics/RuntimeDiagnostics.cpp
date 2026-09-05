#include <Frigga/Diagnostics/RuntimeDiagnostics.hpp>

#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>

#if defined(__unix__) || defined(__APPLE__)
#    include <execinfo.h>
#endif

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        std::filesystem::path &CrashPath()
        {
            static std::filesystem::path path;
            return path;
        }

        void WriteCrashReport(int signalNumber)
        {
            const auto path = CrashPath();
            if(path.empty())
            {
                std::_Exit(128 + signalNumber);
            }
            std::ofstream file(path, std::ios::app);
            file << "Frigga crash report\nsignal=" << signalNumber << "\n";
#if defined(__unix__) || defined(__APPLE__)
            void *frames[64] {};
            const int count = ::backtrace(frames, 64);
            char **symbols   = ::backtrace_symbols(frames, count);
            if(symbols)
            {
                for(int i = 0; i < count; ++i)
                {
                    file << symbols[i] << '\n';
                }
                std::free(symbols);
            }
#endif
            file.flush();
            std::_Exit(128 + signalNumber);
        }
    } // namespace

    void CrashReporter::Install(std::filesystem::path reportPath)
    {
        CrashPath() = std::move(reportPath);
        std::signal(SIGABRT, WriteCrashReport);
        std::signal(SIGFPE, WriteCrashReport);
        std::signal(SIGILL, WriteCrashReport);
        std::signal(SIGSEGV, WriteCrashReport);
    }

    FrameProfiler::FrameProfiler(std::filesystem::path tracePath)
        : mTracePath(std::move(tracePath)), mStarted(std::chrono::steady_clock::now())
    {
    }

    FrameProfiler::~FrameProfiler()
    {
        if(mTracePath.empty())
        {
            return;
        }
        std::error_code ec;
        std::filesystem::create_directories(mTracePath.parent_path(), ec);
        std::ofstream file(mTracePath, std::ios::trunc);
        if(!file)
        {
            return;
        }
        file << "{\"traceEvents\":[";
        std::lock_guard lock(mMutex);
        for(std::size_t i = 0; i < mEvents.size(); ++i)
        {
            const auto &event = mEvents[i];
            if(i != 0)
            {
                file << ',';
            }
            file << "{\"name\":\"";
            for(const char ch : event.name)
            {
                if(ch == '"' || ch == '\\')
                {
                    file << '\\';
                }
                file << ch;
            }
            file << "\",\"cat\":\"frigga\",\"ph\":\"X\",\"ts\":" << event.timestampUs
                 << ",\"dur\":" << event.durationUs << ",\"pid\":1,\"tid\":1}";
        }
        file << "]}\n";
    }

    void FrameProfiler::Record(std::string name, std::chrono::steady_clock::duration duration)
    {
        if(mTracePath.empty())
        {
            return;
        }
        const auto now = std::chrono::steady_clock::now();
        const auto timestamp =
            std::chrono::duration_cast<std::chrono::microseconds>(now - mStarted).count();
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
        std::lock_guard lock(mMutex);
        if(mEvents.size() < 10000)
        {
            mEvents.push_back(Event {.name = std::move(name),
                                     .timestampUs = timestamp,
                                     .durationUs = elapsed});
        }
    }
} // namespace FRIGGA_NAMESPACE
