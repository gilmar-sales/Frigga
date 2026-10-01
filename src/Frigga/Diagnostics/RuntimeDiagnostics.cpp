#include <Frigga/Diagnostics/RuntimeDiagnostics.hpp>

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <typeinfo>
#include <utility>

#if defined(_WIN32)

#include <windows.h>

#include <dbghelp.h>
#else
#include <unistd.h>
#endif

#if defined(__unix__) || defined(__APPLE__)
#include <cxxabi.h>
#include <execinfo.h>
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

        struct CrashContext
        {
            std::mutex mutex;
            std::vector<std::pair<std::string, std::string>> values;
            std::vector<std::string> breadcrumbs;
        };

        CrashContext &Context()
        {
            static CrashContext context;
            return context;
        }

        void AppendCrashContext(std::ostringstream &out)
        {
            auto &context = Context();
            std::unique_lock lock(context.mutex, std::try_to_lock);
            if(!lock.owns_lock())
            {
                out << "<context unavailable: update in progress>\n";
                return;
            }
            if(!context.values.empty())
            {
                out << "--- context ---\n";
                for(const auto &[key, value]: context.values)
                {
                    out << key << ": " << value << '\n';
                }
            }
            if(!context.breadcrumbs.empty())
            {
                out << "--- recent events ---\n";
                for(const auto &breadcrumb: context.breadcrumbs)
                {
                    out << "- " << breadcrumb << '\n';
                }
            }
        }

        std::atomic_flag &CrashGuard()
        {
            static std::atomic_flag guard;
            return guard;
        }

        const char *BuildType()
        {
#if defined(NDEBUG)
            return "Release";
#else
            return "Debug";
#endif
        }

        const char *PlatformName()
        {
#if defined(_WIN32)
            return "Windows x64";
#elif defined(__APPLE__)
            return "macOS";
#elif defined(__linux__)
            return "Linux";
#else
            return "unknown";
#endif
        }

        std::string CurrentTimestamp()
        {
            const auto now         = std::chrono::system_clock::now();
            const std::time_t time = std::chrono::system_clock::to_time_t(now);
            std::tm tmValue{};
#if defined(_WIN32)
            gmtime_s(&tmValue, &time);
#else
            gmtime_r(&time, &tmValue);
#endif
            std::ostringstream out;
            out << std::put_time(&tmValue, "%Y-%m-%dT%H:%M:%SZ");
            return out.str();
        }

        std::string CurrentModulePath()
        {
#if defined(_WIN32)
            char buffer[MAX_PATH]{};
            const DWORD length = GetModuleFileNameA(nullptr, buffer, sizeof(buffer));
            if(length > 0 && length < sizeof(buffer))
            {
                return {buffer, length};
            }
            return "<unknown>";
#else
#if defined(__linux__)
            char buffer[4096]{};
            const ssize_t length = ::readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
            if(length > 0)
            {
                return {buffer, static_cast<std::size_t>(length)};
            }
#endif
            return "<unknown>";
#endif
        }

        long CurrentProcessId()
        {
#if defined(_WIN32)
            return static_cast<long>(GetCurrentProcessId());
#else
            return static_cast<long>(::getpid());
#endif
        }

#if defined(_WIN32)
        void WriteMiniDump(EXCEPTION_POINTERS *exceptionPointers)
        {
            auto dumpPath = CrashPath();
            dumpPath += ".dmp";
            std::error_code ec;
            if(const auto parent = dumpPath.parent_path(); !parent.empty())
            {
                std::filesystem::create_directories(parent, ec);
            }
            HANDLE dumpFile = CreateFileW(dumpPath.c_str(), GENERIC_WRITE, 0, nullptr,
                                          CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if(dumpFile == INVALID_HANDLE_VALUE)
            {
                return;
            }
            MINIDUMP_EXCEPTION_INFORMATION exceptionInfo{
                .ThreadId          = GetCurrentThreadId(),
                .ExceptionPointers = exceptionPointers,
                .ClientPointers    = FALSE,
            };
            const auto dumpType =
                static_cast<MINIDUMP_TYPE>(MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);
            MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dumpFile, dumpType,
                              exceptionPointers != nullptr ? &exceptionInfo : nullptr, nullptr,
                              nullptr);
            CloseHandle(dumpFile);
        }

        void AppendWindowsStackTrace(std::ostringstream &out)
        {
            void *frames[62]{};
            const WORD count = CaptureStackBackTrace(0, 62, frames, nullptr);
            if(count == 0)
            {
                out << "<stack trace unavailable>\n";
                return;
            }

            const HANDLE process = GetCurrentProcess();
            static std::atomic_flag symGuard;
            if(!symGuard.test_and_set())
            {
                SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
                SymInitialize(process, nullptr, TRUE);
            }

            alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 256 * sizeof(char)]{};
            auto *symbol         = reinterpret_cast<SYMBOL_INFO *>(buffer);
            symbol->MaxNameLen   = 255;
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

            for(WORD i = 0; i < count; ++i)
            {
                const auto address     = reinterpret_cast<DWORD64>(frames[i]);
                DWORD64 displacement   = 0;
                IMAGEHLP_LINE64 line   = {};
                line.SizeOfStruct      = sizeof(line);
                DWORD lineDisplacement = 0;
                const bool hasSymbol   = SymFromAddr(process, address, &displacement, symbol);
                const bool hasLine =
                    SymGetLineFromAddr64(process, address, &lineDisplacement, &line);
                out << "#" << i << " 0x" << std::hex << address << std::dec << " ";
                if(hasSymbol)
                {
                    out << symbol->Name << "+0x" << std::hex << displacement << std::dec;
                }
                else
                {
                    out << "<unknown symbol>";
                }
                if(hasLine)
                {
                    out << " (" << line.FileName << ":" << line.LineNumber << ")";
                }
                out << '\n';
            }
        }

        std::string_view DescribeSehCode(DWORD code)
        {
            switch(code)
            {
            case EXCEPTION_ACCESS_VIOLATION:
                return "EXCEPTION_ACCESS_VIOLATION — null or invalid pointer dereference";
            case EXCEPTION_INT_DIVIDE_BY_ZERO:
                return "EXCEPTION_INT_DIVIDE_BY_ZERO — integer divide-by-zero (maps to SIGFPE)";
            case EXCEPTION_FLT_DIVIDE_BY_ZERO:
                return "EXCEPTION_FLT_DIVIDE_BY_ZERO — floating-point divide-by-zero";
            case EXCEPTION_STACK_OVERFLOW:
                return "EXCEPTION_STACK_OVERFLOW — stack exhaustion (often unbounded recursion)";
            case EXCEPTION_ILLEGAL_INSTRUCTION:
                return "EXCEPTION_ILLEGAL_INSTRUCTION — illegal CPU instruction";
            case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
                return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED — out-of-bounds access";
            default:
                return "unknown structured exception";
            }
        }

        int SehCodeToSignal(DWORD code)
        {
            switch(code)
            {
            case EXCEPTION_INT_DIVIDE_BY_ZERO:
            case EXCEPTION_FLT_DIVIDE_BY_ZERO:
                return SIGFPE;
            case EXCEPTION_ILLEGAL_INSTRUCTION:
                return SIGILL;
            case EXCEPTION_ACCESS_VIOLATION:
            case EXCEPTION_STACK_OVERFLOW:
            case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
                return SIGSEGV;
            default:
                return SIGABRT;
            }
        }
#endif

#if defined(__unix__) || defined(__APPLE__)
        void AppendPosixStackTrace(std::ostringstream &out)
        {
            void *frames[64]{};
            const int count = ::backtrace(frames, 64);
            char **symbols  = ::backtrace_symbols(frames, count);
            if(symbols == nullptr)
            {
                for(int i = 0; i < count; ++i)
                {
                    out << "#" << i << " " << frames[i] << '\n';
                }
                return;
            }
            for(int i = 0; i < count; ++i)
            {
                const std::string_view symbol(symbols[i]);
                // backtrace_symbols format: "module(mangled+0xoffset) [addr]".
                // Demangle the interior so C++ frames stay readable.
                const auto begin = symbol.find('(');
                const auto plus  = symbol.find('+', begin == std::string_view::npos ? 0 : begin);
                if(begin != std::string_view::npos && plus != std::string_view::npos &&
                   plus > begin + 1)
                {
                    const std::string mangled(symbol.substr(begin + 1, plus - begin - 1));
                    int status = 0;
                    char *demangled =
                        abi::__cxa_demangle(mangled.c_str(), nullptr, nullptr, &status);
                    if(status == 0 && demangled != nullptr)
                    {
                        out << symbol.substr(0, begin + 1) << demangled << symbol.substr(plus)
                            << '\n';
                        std::free(demangled);
                        continue;
                    }
                    std::free(demangled);
                }
                out << symbol << '\n';
            }
            std::free(symbols);
        }
#endif

        void WriteReport(int signalNumber, const std::string &reason,
                         const std::string &extraContext)
        {
            // Best effort only: a second fault while reporting must not recurse forever.
            if(CrashGuard().test_and_set())
            {
                std::_Exit(128 + signalNumber);
            }

            const auto path = CrashPath();

            std::ostringstream out;
            out << "\n=========== Frigga crash report ===========\n";
            out << "timestamp: " << CurrentTimestamp() << '\n';
            out << "signal: " << signalNumber << " (" << CrashReporter::DescribeSignal(signalNumber)
                << ")\n";
            out << "process: pid=" << CurrentProcessId() << " tid=" << std::this_thread::get_id()
                << '\n';
            out << "module: " << CurrentModulePath() << '\n';
            out << "build: " << PlatformName() << " " << BuildType() << " " << __DATE__ << " "
                << __TIME__ << '\n';
#ifdef FRIGGA_VERSION_STRING
            out << "version: " << FRIGGA_VERSION_STRING << '\n';
#endif
            if(!reason.empty())
            {
                out << "reason: " << reason << '\n';
            }
            if(!extraContext.empty())
            {
                out << extraContext;
            }
            AppendCrashContext(out);
            out << "--- stack ---\n";
#if defined(_WIN32)
            AppendWindowsStackTrace(out);
#elif defined(__unix__) || defined(__APPLE__)
            AppendPosixStackTrace(out);
#else
            out << "<stack trace not supported on this platform>\n";
#endif
            out << "===========================================\n";
            out.flush();

            // Also echo to stderr: when stdout is captured by an IDE the file alone is easy to
            // miss.
            std::fputs(out.str().c_str(), stderr);

            if(!path.empty())
            {
                std::error_code ec;
                if(const auto parent = path.parent_path(); !parent.empty())
                {
                    std::filesystem::create_directories(parent, ec);
                }
                if(std::ofstream file(path, std::ios::app); file)
                {
                    file << out.str();
                    file.flush();
                }
            }
            std::_Exit(128 + signalNumber);
        }

        void WriteCrashReport(int signalNumber)
        {
#if defined(_WIN32)
            WriteMiniDump(nullptr);
#endif
            WriteReport(signalNumber, {}, {});
        }

        void TerminateHandler()
        {
            std::string reason = "uncaught C++ exception";
            try
            {
                if(const auto exception = std::current_exception())
                {
                    try
                    {
                        std::rethrow_exception(exception);
                    }
                    catch(const std::exception &error)
                    {
                        reason += std::string(" (") + typeid(error).name() + "): " + error.what();
                    }
                    catch(...)
                    {
                        reason += " (non-std::exception type)";
                    }
                }
                else
                {
                    reason += " (no active exception: terminate called directly)";
                }
            }
            catch(...)
            {
                // Keep the original best-effort report even if introspection fails.
            }
#if defined(_WIN32)
            WriteMiniDump(nullptr);
#endif
            WriteReport(SIGABRT, reason, {});
            std::_Exit(SIGABRT);
        }

#if defined(_WIN32)
        LONG WINAPI SehFilter(EXCEPTION_POINTERS *info)
        {
            std::string extra;
            std::string reason;
            int signalNumber = SIGABRT;
            if(info != nullptr && info->ExceptionRecord != nullptr)
            {
                const DWORD code = info->ExceptionRecord->ExceptionCode;
                signalNumber     = SehCodeToSignal(code);
                char hex[16]{};
                std::snprintf(hex, sizeof(hex), "%08lX", static_cast<unsigned long>(code));
                reason = std::string("windows structured exception 0x") + hex + " (" +
                         std::string(DescribeSehCode(code)) + ")";
                std::ostringstream context;
                context << "fault-address: 0x" << std::hex
                        << reinterpret_cast<std::uintptr_t>(info->ExceptionRecord->ExceptionAddress)
                        << std::dec << '\n';
                if(info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
                   info->ExceptionRecord->NumberParameters >= 2)
                {
                    context << "access: "
                            << (info->ExceptionRecord->ExceptionInformation[0] == 0 ? "read"
                                                                                    : "write")
                            << " at 0x" << std::hex
                            << info->ExceptionRecord->ExceptionInformation[1] << std::dec << '\n';
                }
                extra = context.str();
            }
            WriteMiniDump(info);
            WriteReport(signalNumber, reason, extra);
            return EXCEPTION_EXECUTE_HANDLER;
        }
#endif
    } // namespace

    std::string_view CrashReporter::DescribeSignal(int signalNumber)
    {
        switch(signalNumber)
        {
        case SIGABRT:
            return "SIGABRT — abort (failed assert/check, terminate, std::abort)";
        case SIGFPE:
            return "SIGFPE — arithmetic fault (integer divide-by-zero, float exception)";
        case SIGILL:
            return "SIGILL — illegal instruction (corrupt code/stack, bad jump)";
        case SIGSEGV:
            return "SIGSEGV — access violation (null or dangling pointer dereference)";
#ifdef SIGBUS
        case SIGBUS:
            return "SIGBUS — bus error (misaligned or unmapped memory access)";
#endif
        case SIGTERM:
            return "SIGTERM — termination request";
        case SIGINT:
            return "SIGINT — interrupted (Ctrl+C)";
        default:
            return "unknown signal";
        }
    }

    void CrashReporter::Install(std::filesystem::path reportPath)
    {
        CrashPath() = std::move(reportPath);
#if defined(_WIN32)
        // Keep Windows SEH intact so the unhandled-exception filter receives the exception
        // pointers needed for a useful dump. SIGABRT is retained for explicit abort() calls.
        std::signal(SIGABRT, WriteCrashReport);
#else
        std::signal(SIGABRT, WriteCrashReport);
        std::signal(SIGFPE, WriteCrashReport);
        std::signal(SIGILL, WriteCrashReport);
        std::signal(SIGSEGV, WriteCrashReport);
#endif
        std::set_terminate(TerminateHandler);
#if defined(_WIN32)
        SetUnhandledExceptionFilter(SehFilter);
#endif
        SetContext("process_id", std::to_string(CurrentProcessId()));
        SetContext("module", CurrentModulePath());
        SetContext("platform", PlatformName());
        SetContext("build_type", BuildType());
        AddBreadcrumb("Crash reporting initialized");
    }

    void CrashReporter::SetContext(std::string key, std::string value)
    {
        auto &context = Context();
        std::lock_guard lock(context.mutex);
        for(auto &[existingKey, existingValue]: context.values)
        {
            if(existingKey == key)
            {
                existingValue = std::move(value);
                return;
            }
        }
        context.values.emplace_back(std::move(key), std::move(value));
    }

    void CrashReporter::AddBreadcrumb(std::string_view message)
    {
        auto &context = Context();
        std::lock_guard lock(context.mutex);
        constexpr std::size_t maxBreadcrumbs = 64;
        if(context.breadcrumbs.size() == maxBreadcrumbs)
        {
            context.breadcrumbs.erase(context.breadcrumbs.begin());
        }
        context.breadcrumbs.emplace_back(message);
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
        std::string buffer;
        buffer.reserve(mEvents.size() * 96);
        std::lock_guard lock(mMutex);
        for(std::size_t i = 0; i < mEvents.size(); ++i)
        {
            const auto &event = mEvents[i];
            if(i != 0)
            {
                buffer += ',';
            }
            buffer += "{\"name\":\"";
            for(const char ch: event.name)
            {
                if(ch == '"' || ch == '\\')
                {
                    buffer += '\\';
                }
                buffer += ch;
            }
            buffer += "\",\"cat\":\"frigga\",\"ph\":\"X\",\"ts\":";
            buffer += std::to_string(event.timestampUs);
            buffer += ",\"dur\":";
            buffer += std::to_string(event.durationUs);
            buffer += ",\"pid\":1,\"tid\":1}";
        }
        file << buffer;
        file << "]}\n";
    }

    void FrameProfiler::Record(std::string_view name, std::chrono::steady_clock::duration duration)
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
            mEvents.push_back(
                Event{.name = std::string(name), .timestampUs = timestamp, .durationUs = elapsed});
        }
    }
} // namespace FRIGGA_NAMESPACE
