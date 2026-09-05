#pragma once

#include <functional>
#include <string>
#include <string_view>

namespace frigga::mcp
{
    class IMcpTransport
    {
      public:
        using MessageHandler = std::function<std::string(std::string_view)>;

        virtual ~IMcpTransport() = default;
        virtual bool Start(MessageHandler handler, std::string &error) = 0;
        virtual void Stop() = 0;
    };
} // namespace frigga::mcp
