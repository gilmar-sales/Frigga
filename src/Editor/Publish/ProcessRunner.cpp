#include "ProcessRunner.hpp"

#include <array>
#include <cstdio>
#include <string>

#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace publish
{
    int SystemProcessRunner::Run(const std::string &command,
                                 const std::function<void(std::string_view)> &onLine)
    {
        const std::string wrapped = command + " 2>&1";
#ifdef _WIN32
        FILE *pipe = _popen(wrapped.c_str(), "r");
#else
        FILE *pipe = popen(wrapped.c_str(), "r");
#endif
        if(!pipe)
        {
            return -1;
        }

        std::array<char, 512> buffer {};
        while(fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
        {
            onLine(buffer.data());
        }

#ifdef _WIN32
        return _pclose(pipe);
#else
        const int status = pclose(pipe);
        if(status == -1)
        {
            return -1;
        }
        if(WIFEXITED(status))
        {
            return WEXITSTATUS(status);
        }
        return -1;
#endif
    }

    bool TryParseNinjaProgress(std::string_view line, float &outProgress)
    {
        const auto open = line.find('[');
        if(open == std::string_view::npos)
        {
            return false;
        }
        const auto slash = line.find('/', open + 1);
        if(slash == std::string_view::npos)
        {
            return false;
        }
        const auto close = line.find(']', slash + 1);
        if(close == std::string_view::npos)
        {
            return false;
        }

        int current = 0;
        int total   = 0;
        try
        {
            current = std::stoi(std::string(line.substr(open + 1, slash - open - 1)));
            total   = std::stoi(std::string(line.substr(slash + 1, close - slash - 1)));
        }
        catch(...)
        {
            return false;
        }

        if(total <= 0)
        {
            return false;
        }
        outProgress = static_cast<float>(current) / static_cast<float>(total);
        return true;
    }
} // namespace publish
