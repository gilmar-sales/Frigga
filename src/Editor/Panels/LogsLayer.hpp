#pragma once

#include <Frigga/Core/Layer.hpp>

#include <Skirnir/Common/Arc.hpp>
#include <Skirnir/Logging/LogLevel.hpp>
#include <Skirnir/Logging/LogRecord.hpp>
#include <Skirnir/Logging/LogSinks/ILogSink.hpp>
#include <Skirnir/Logging/Logger.hpp>

#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <string>

class LogsLayer: public fg::Layer,
                 public skr::ILogSink,
                 public skr::enable_arc_from_this<LogsLayer>
{
  public:
    explicit LogsLayer(skr::Arc<skr::LoggerOptions> loggerOptions);
    ~LogsLayer() override = default;

    void onAttach() override;
    void Write(const skr::LogRecord &record) override;
    void onGui() override;

  private:
    struct Entry
    {
        skr::LogLevel level;
        std::string   timestamp;
        std::string   category;
        std::string   message;
    };

    void drawToolbar();
    void drawList();

    skr::Arc<skr::LoggerOptions> mLoggerOptions;

    std::mutex        mMutex;
    std::deque<Entry> mEntries;
    std::size_t       mMaxEntries = 1000;

    bool mAutoScroll  = true;
    bool mShowTimestamp = true;
    bool mFilterError = true;
    bool mFilterWarn  = true;
    bool mFilterInfo  = true;
    bool mFilterDebug = false;
    bool mFilterTrace = false;
    int  mLevelIndex  = 0;
};
