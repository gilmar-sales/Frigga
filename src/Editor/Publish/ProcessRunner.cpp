#include "ProcessRunner.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

namespace publish
{
    namespace
    {
        std::string ToUtf8(const std::wstring &wide)
        {
#ifdef _WIN32
            if(wide.empty())
            {
                return {};
            }
            const int needed =
                WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                                    nullptr, 0, nullptr, nullptr);
            std::string out(static_cast<std::size_t>(needed), '\0');
            WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                                out.data(), needed, nullptr, nullptr);
            return out;
#else
            (void)wide;
            return {};
#endif
        }

        std::wstring ToWide(std::string_view utf8)
        {
#ifdef _WIN32
            if(utf8.empty())
            {
                return {};
            }
            const int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                                                   static_cast<int>(utf8.size()), nullptr, 0);
            std::wstring out(static_cast<std::size_t>(needed), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                                out.data(), needed);
            return out;
#else
            (void)utf8;
            return {};
#endif
        }

        void AppendCapped(std::string &output, bool &truncated, std::size_t cap,
                          std::string_view chunk)
        {
            if(cap == 0)
            {
                return;
            }
            if(output.size() + chunk.size() <= cap)
            {
                output.append(chunk);
                return;
            }
            truncated = true;
            const std::size_t keepTail = cap;
            std::string merged;
            merged.reserve(keepTail);
            const std::string incoming(output);
            const std::string extra(chunk);
            const std::string all = incoming + extra;
            merged.assign(all.substr(all.size() - keepTail));
            output = std::move(merged);
        }

        void EmitLines(std::string &pending, std::string_view chunk,
                       const std::function<void(std::string_view)> &onLine,
                       std::string &output, bool &truncated, std::size_t cap)
        {
            AppendCapped(output, truncated, cap, chunk);
            pending.append(chunk);
            std::size_t pos = 0;
            while(true)
            {
                const auto nl = pending.find('\n', pos);
                if(nl == std::string::npos)
                {
                    break;
                }
                std::string_view line(pending.data() + pos, nl - pos + 1);
                if(onLine)
                {
                    onLine(line);
                }
                pos = nl + 1;
            }
            if(pos > 0)
            {
                pending.erase(0, pos);
            }
        }
    } // namespace

    ProcessResult IProcessRunner::Run(const ProcessOptions &options,
                                      const std::function<void(std::string_view)> &onLine)
    {
        const auto begin = std::chrono::steady_clock::now();
        ProcessResult result;
        result.started = true;
        std::string pending;
        const int code = Run(options.command, [&](std::string_view line) {
            AppendCapped(result.output, result.truncated, options.maxOutputBytes, line);
            pending.append(line);
            if(onLine)
            {
                onLine(line);
            }
        });
        // Run(string) already streams whole chunks; output collected above.
        result.exitCode = code;
        result.started  = code != -1;
        const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - begin;
        result.seconds                              = elapsed.count();
        return result;
    }

    bool IProcessRunner::LaunchDetached(const std::string &,
                                        const std::filesystem::path &)
    {
        return false;
    }

#ifdef _WIN32
    namespace
    {
        std::wstring ComSpec()
        {
            wchar_t buffer[MAX_PATH];
            const DWORD len = GetEnvironmentVariableW(L"COMSPEC", buffer, MAX_PATH);
            if(len > 0 && len < MAX_PATH)
            {
                return buffer;
            }
            return L"cmd.exe";
        }

        std::wstring ShellCommandLine(const std::string &command)
        {
            // cmd.exe /S /C "<command>" preserves _popen semantics without a window.
            return ComSpec() + L" /S /C \"" + ToWide(command) + L"\"";
        }
    } // namespace

    int SystemProcessRunner::Run(const std::string &command,
                                 const std::function<void(std::string_view)> &onLine)
    {
        ProcessOptions options;
        options.command = command;
        return Run(options, onLine).exitCode;
    }

    ProcessResult SystemProcessRunner::Run(
        const ProcessOptions &options, const std::function<void(std::string_view)> &onLine)
    {
        const auto begin = std::chrono::steady_clock::now();
        ProcessResult result;

        SECURITY_ATTRIBUTES sa {};
        sa.nLength              = sizeof(sa);
        sa.bInheritHandle       = TRUE;
        sa.lpSecurityDescriptor = nullptr;

        HANDLE readPipe  = nullptr;
        HANDLE writePipe = nullptr;
        if(!CreatePipe(&readPipe, &writePipe, &sa, 0) || !readPipe || !writePipe)
        {
            if(readPipe)
            {
                CloseHandle(readPipe);
            }
            if(writePipe)
            {
                CloseHandle(writePipe);
            }
            result.exitCode = -1;
            result.started  = false;
            return result;
        }
        // Parent must not inherit the read end, otherwise ReadFile never sees EOF.
        SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

        HANDLE nulInput = CreateFileW(L"NUL", GENERIC_READ,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL, nullptr);

        STARTUPINFOW si {};
        si.cb         = sizeof(si);
        si.dwFlags    = STARTF_USESTDHANDLES;
        si.hStdOutput = writePipe;
        si.hStdError  = options.mergeStderr ? writePipe : GetStdHandle(STD_ERROR_HANDLE);
        si.hStdInput  = nulInput != INVALID_HANDLE_VALUE ? nulInput : nullptr;

        std::wstring cmdLine = ShellCommandLine(options.command);
        const wchar_t *workDir =
            options.workDir.empty() ? nullptr : options.workDir.c_str();

        PROCESS_INFORMATION pi {};
        DWORD flags = CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT;
        if(!options.hideWindow)
        {
            flags &= static_cast<DWORD>(~CREATE_NO_WINDOW);
        }
        const BOOL spawned = CreateProcessW(nullptr, cmdLine.data(), nullptr, nullptr, TRUE,
                                            flags, nullptr, workDir, &si, &pi);
        // Parent closes its write end + NUL handle; child keeps its copies.
        CloseHandle(writePipe);
        if(nulInput != INVALID_HANDLE_VALUE && nulInput != nullptr)
        {
            CloseHandle(nulInput);
        }
        if(!spawned)
        {
            CloseHandle(readPipe);
            result.exitCode = -1;
            result.started  = false;
            const std::chrono::duration<double> elapsed =
                std::chrono::steady_clock::now() - begin;
            result.seconds = elapsed.count();
            return result;
        }
        result.started = true;
        CloseHandle(pi.hThread);

        std::string pending;
        std::array<char, 4096> buffer {};
        DWORD bytesRead = 0;
        while(ReadFile(readPipe, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead,
                       nullptr) &&
              bytesRead > 0)
        {
            EmitLines(pending, std::string_view(buffer.data(), bytesRead), onLine,
                      result.output, result.truncated, options.maxOutputBytes);
        }
        if(!pending.empty())
        {
            if(onLine)
            {
                onLine(pending);
            }
            AppendCapped(result.output, result.truncated, options.maxOutputBytes, pending);
            pending.clear();
        }
        CloseHandle(readPipe);

        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD code = 1;
        if(!GetExitCodeProcess(pi.hProcess, &code))
        {
            code = 1;
        }
        CloseHandle(pi.hProcess);

        result.exitCode = static_cast<int>(code);
        const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - begin;
        result.seconds                              = elapsed.count();
        return result;
    }

    bool SystemProcessRunner::LaunchDetached(const std::string &command,
                                             const std::filesystem::path &workDir)
    {
        STARTUPINFOW si {};
        si.cb      = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION pi {};
        std::wstring cmdLine = ShellCommandLine(command);
        const wchar_t *dir   = workDir.empty() ? nullptr : workDir.c_str();
        const BOOL spawned =
            CreateProcessW(nullptr, cmdLine.data(), nullptr, nullptr, FALSE,
                           CREATE_NO_WINDOW | DETACHED_PROCESS | CREATE_UNICODE_ENVIRONMENT,
                           nullptr, dir, &si, &pi);
        if(!spawned)
        {
            return false;
        }
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return true;
    }
#else
    int SystemProcessRunner::Run(const std::string &command,
                                 const std::function<void(std::string_view)> &onLine)
    {
        ProcessOptions options;
        options.command = command;
        return Run(options, onLine).exitCode;
    }

    ProcessResult SystemProcessRunner::Run(
        const ProcessOptions &options, const std::function<void(std::string_view)> &onLine)
    {
        const auto begin = std::chrono::steady_clock::now();
        ProcessResult result;
        result.started = true;

        std::string shell = options.command;
        if(!options.workDir.empty())
        {
            shell = "cd \"" + options.workDir.string() + "\" && (" + shell + ")";
        }
        if(options.mergeStderr)
        {
            shell += " 2>&1";
        }
        FILE *pipe = popen(shell.c_str(), "r");
        if(!pipe)
        {
            result.exitCode = -1;
            result.started  = false;
            return result;
        }

        std::string pending;
        std::array<char, 4096> buffer {};
        while(fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
        {
            EmitLines(pending, std::string_view(buffer.data()), onLine, result.output,
                      result.truncated, options.maxOutputBytes);
        }
        if(!pending.empty())
        {
            if(onLine)
            {
                onLine(pending);
            }
            AppendCapped(result.output, result.truncated, options.maxOutputBytes, pending);
        }

        const int status = pclose(pipe);
        if(status == -1)
        {
            result.exitCode = -1;
            result.started  = false;
        }
        else if(WIFEXITED(status))
        {
            result.exitCode = WEXITSTATUS(status);
        }
        else
        {
            result.exitCode = -1;
        }
        const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - begin;
        result.seconds                              = elapsed.count();
        (void)ToUtf8(L"");
        (void)ToWide("");
        return result;
    }

    bool SystemProcessRunner::LaunchDetached(const std::string &command,
                                             const std::filesystem::path &workDir)
    {
        std::string shell = command;
        if(!workDir.empty())
        {
            shell = "cd \"" + workDir.string() + "\" && (" + shell + ")";
        }
        shell += " >/dev/null 2>&1 &";
        pid_t pid         = 0;
        char shBin[]      = "/bin/sh";
        char shArg[]      = "sh";
        char dashC[]      = "-c";
        std::string shellMut = shell;
        char *const argv[]   = {shArg, dashC, shellMut.data(), nullptr};
        if(posix_spawn(&pid, shBin, nullptr, nullptr, argv, environ) == 0)
        {
            return true;
        }
        return std::system(shell.c_str()) == 0;
    }
#endif

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
