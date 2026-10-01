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
#include <vector>

class ProjectSession;
struct EditorBackgroundTask;

class LogsLayer: public fg::Layer,
                 public skr::ILogSink,
                 public skr::enable_arc_from_this<LogsLayer>
{
  public:
    explicit LogsLayer(skr::Arc<skr::LoggerOptions> loggerOptions,
                       skr::Arc<ProjectSession> session = {});
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

    void drawToolbar(const std::vector<std::string> &sources);
    void drawList(const std::string &categoryFilter);
    void drawTaskView(const EditorBackgroundTask &task);

    skr::Arc<skr::LoggerOptions> mLoggerOptions;
    skr::Arc<ProjectSession> mSession;

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
    /// 0 = Editor logs, 1..N = background task selected from the source combo.
    int mSourceIndex   = 0;
    /// 0 = all categories, 1..N = single category (what is currently executing).
    int mCategoryIndex = 0;
};
