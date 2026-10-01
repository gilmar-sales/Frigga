#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace publish
{
    /// Options for a single external tool invocation (cmake, ninja, code editor…).
    /// The runner never shows a console window (Windows: CREATE_NO_WINDOW).
    struct ProcessOptions
    {
        std::string command;
        /// Empty = inherit the current working directory.
        std::filesystem::path workDir;
        /// Merge stderr into the captured/stdout stream (default true, matches `2>&1`).
        bool mergeStderr = true;
        /// Windows only: hide the console window. Always true unless debugging the child.
        bool hideWindow = true;
        /// Cap for ProcessResult::output; older bytes are dropped, @see truncated.
        std::size_t maxOutputBytes = 256 * 1024;
    };

    /// Simplified feedback for a finished process: exit code, collected logs and timing.
    struct ProcessResult
    {
        /// Process exit code, or -1 when it could not be started / waited on.
        int exitCode = -1;
        /// True when the child was spawned (even if it exited non-zero).
        bool started = false;
        /// Combined stdout (+stderr when merged), capped at ProcessOptions::maxOutputBytes.
        std::string output;
        /// True when output exceeded the cap and the head was dropped.
        bool truncated = false;
        /// Wall time in seconds.
        double seconds = 0.0;

        [[nodiscard]] bool ok() const
        {
            return started && exitCode == 0;
        }
    };

    /// Runs external tools (cmake, ninja). Injectable so pipelines can be tested
    /// without spawning processes.
    class IProcessRunner
    {
      public:
        virtual ~IProcessRunner() = default;

        /// Runs @p command through the shell, streaming stdout+stderr to @p onLine.
        /// Returns the process exit code, or -1 when it could not be started.
        /// Never opens a terminal/console window.
        virtual int Run(const std::string &command,
                        const std::function<void(std::string_view)> &onLine) = 0;

        /// Simplified API: run with options, stream lines while collecting the full
        /// log + exit code + duration in one ProcessResult. Default implementation
        /// delegates to Run(command, onLine) so existing fakes keep compiling.
        virtual ProcessResult Run(const ProcessOptions &options,
                                  const std::function<void(std::string_view)> &onLine = {});

        /// Fire-and-forget launch (e.g. "open in code editor") without any console
        /// window and without waiting. Returns false when the spawn failed.
        virtual bool LaunchDetached(const std::string &command,
                                    const std::filesystem::path &workDir = {});
    };

    class SystemProcessRunner final : public IProcessRunner
    {
      public:
        int Run(const std::string &command,
                const std::function<void(std::string_view)> &onLine) override;
        ProcessResult Run(const ProcessOptions &options,
                          const std::function<void(std::string_view)> &onLine = {}) override;
        bool LaunchDetached(const std::string &command,
                            const std::filesystem::path &workDir = {}) override;
    };

    /// Parse Ninja-style "[12/74]" progress. Returns true when a fraction was found.
    bool TryParseNinjaProgress(std::string_view line, float &outProgress);
} // namespace publish
