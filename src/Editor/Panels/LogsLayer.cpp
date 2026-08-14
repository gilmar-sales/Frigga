#include "LogsLayer.hpp"

#include "Editor/DockLayout.hpp"
#include "Editor/UiScale.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <imgui.h>

namespace
{
    ImVec4 LevelColor(skr::LogLevel level)
    {
        switch(level)
        {
        case skr::LogLevel::Trace:
            return ImVec4(0.55f, 0.60f, 0.65f, 1.0f);
        case skr::LogLevel::Debug:
            return ImVec4(0.60f, 0.65f, 0.70f, 1.0f);
        case skr::LogLevel::Warning:
            return ImVec4(1.00f, 0.78f, 0.22f, 1.0f);
        case skr::LogLevel::Error:
            return ImVec4(0.95f, 0.35f, 0.35f, 1.0f);
        case skr::LogLevel::Fatal:
            return ImVec4(0.95f, 0.20f, 0.20f, 1.0f);
        case skr::LogLevel::Information:
        default:
            return ImVec4(0.80f, 0.86f, 0.90f, 1.0f);
        }
    }

    std::string FormatTimestamp(const std::chrono::system_clock::time_point &tp)
    {
        using std::chrono::duration_cast;
        using std::chrono::milliseconds;
        using std::chrono::system_clock;

        const auto ms     = duration_cast<milliseconds>(tp.time_since_epoch()).count();
        const auto secs   = ms / 1000;
        const auto millis = static_cast<int>(ms % 1000);

        const std::time_t time = static_cast<std::time_t>(secs);
        std::tm           tm {};
#ifdef _WIN32
        localtime_s(&tm, &time);
#else
        localtime_r(&time, &tm);
#endif

        char buf[32];
        std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d", tm.tm_hour, tm.tm_min,
                      tm.tm_sec, millis);
        return buf;
    }
} // namespace

LogsLayer::LogsLayer(skr::Arc<skr::LoggerOptions> loggerOptions)
    : fg::Layer("Logs"), mLoggerOptions(std::move(loggerOptions))
{
}

void LogsLayer::onAttach()
{
    if(mLoggerOptions)
    {
        mLoggerOptions->AddSink(skr::Arc<skr::ILogSink>(this->shared_from_this()));
    }
}

void LogsLayer::Write(const skr::LogRecord &record)
{
    Entry entry;
    entry.level     = record.level;
    entry.timestamp = FormatTimestamp(record.timestamp);

    const auto lastColon = record.category.find_last_of(':');
    entry.category = lastColon == std::string::npos ? record.category
                                                    : record.category.substr(lastColon + 1);
    entry.message  = record.message;

    std::string scopes;
    for(const auto &scope : record.scopes)
    {
        if(!scopes.empty())
        {
            scopes += "/";
        }
        scopes += scope;
    }
    if(!scopes.empty())
    {
        entry.message = "[" + scopes + "] " + entry.message;
    }

    std::lock_guard<std::mutex> lock(mMutex);
    mEntries.push_back(std::move(entry));
    if(mEntries.size() > mMaxEntries)
    {
        const auto drop = mEntries.size() - mMaxEntries;
        mEntries.erase(mEntries.begin(), mEntries.begin() + static_cast<long>(drop));
    }
}

void LogsLayer::drawToolbar()
{
    if(ImGui::SmallButton("Clear"))
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mEntries.clear();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &mAutoScroll);
    ImGui::SameLine();
    ImGui::Checkbox("Timestamp", &mShowTimestamp);
    ImGui::SameLine();
    ImGui::Checkbox("Error", &mFilterError);
    ImGui::SameLine();
    ImGui::Checkbox("Warn", &mFilterWarn);
    ImGui::SameLine();
    ImGui::Checkbox("Info", &mFilterInfo);
    ImGui::SameLine();
    ImGui::Checkbox("Debug", &mFilterDebug);
    ImGui::SameLine();
    ImGui::Checkbox("Trace", &mFilterTrace);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(EditorUiScale::S(120.0f));
    const int previous = mLevelIndex;
    if(ImGui::Combo("##LevelPreset", &mLevelIndex,
                    "All\0Error+\0Warn+\0Info+\0Debug+\0Trace\0"))
    {
        if(previous != mLevelIndex)
        {
            if(mLevelIndex == 0)
            {
                // "All": show every level.
                mFilterError = mFilterWarn = mFilterInfo = mFilterDebug = mFilterTrace = true;
            }
            else
            {
                // "X+": show level X (Error=1..Trace=5) and anything more severe.
                mFilterError = mLevelIndex >= 1;
                mFilterWarn  = mLevelIndex >= 2;
                mFilterInfo  = mLevelIndex >= 3;
                mFilterDebug = mLevelIndex >= 4;
                mFilterTrace = mLevelIndex >= 5;
            }
        }
    }
}

void LogsLayer::drawList()
{
    std::deque<Entry> snapshot;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        snapshot = mEntries;
    }

    const float availHeight = ImGui::GetContentRegionAvail().y;
    if(ImGui::BeginChild("##LogsList", ImVec2(0, availHeight), false,
                         ImGuiWindowFlags_HorizontalScrollbar))
    {
        std::size_t shown = 0;
        for(const auto &entry : snapshot)
        {
            const bool matches = [&] {
                switch(entry.level)
                {
                case skr::LogLevel::Error:
                case skr::LogLevel::Fatal:
                    return mFilterError;
                case skr::LogLevel::Warning:
                    return mFilterWarn;
                case skr::LogLevel::Information:
                    return mFilterInfo;
                case skr::LogLevel::Debug:
                    return mFilterDebug;
                case skr::LogLevel::Trace:
                    return mFilterTrace;
                default:
                    return true;
                }
            }();
            if(!matches)
            {
                continue;
            }

            ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(entry.level));
            if(mShowTimestamp)
            {
                ImGui::TextUnformatted(entry.timestamp.c_str());
                ImGui::SameLine();
            }
            ImGui::TextDisabled("%s", entry.category.c_str());
            ImGui::SameLine();
            ImGui::TextUnformatted(entry.message.c_str());
            ImGui::PopStyleColor();
            ++shown;
        }

        if(mAutoScroll && shown > 0)
        {
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::EndChild();
}

void LogsLayer::onGui()
{
    const auto windowId = EditorDock::WindowId("Logs");
    if(!ImGui::Begin(windowId.c_str()))
    {
        ImGui::End();
        return;
    }

    drawToolbar();
    ImGui::Separator();
    drawList();

    ImGui::End();
}
