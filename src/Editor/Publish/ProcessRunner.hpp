#pragma once

#include <functional>
#include <string>
#include <string_view>

namespace publish
{
    /// Runs external tools (cmake, ninja). Injectable so pipelines can be tested
    /// without spawning processes.
    class IProcessRunner
    {
      public:
        virtual ~IProcessRunner() = default;

        /// Runs @p command through the shell, streaming stdout+stderr to @p onLine.
        /// Returns the process exit code, or -1 when it could not be started.
        virtual int Run(const std::string &command,
                        const std::function<void(std::string_view)> &onLine) = 0;
    };

    class SystemProcessRunner final : public IProcessRunner
    {
      public:
        int Run(const std::string &command,
                const std::function<void(std::string_view)> &onLine) override;
    };

    /// Parse Ninja-style "[12/74]" progress. Returns true when a fraction was found.
    bool TryParseNinjaProgress(std::string_view line, float &outProgress);
} // namespace publish
