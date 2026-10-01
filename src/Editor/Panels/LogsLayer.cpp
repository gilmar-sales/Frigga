#include "LogsLayer.hpp"

#include "Editor/DockLayout.hpp"
#include "Editor/Project/ProjectSession.hpp"
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

LogsLayer::LogsLayer(skr::Arc<skr::LoggerOptions> loggerOptions,
                     skr::Arc<ProjectSession> session)
    : fg::Layer("Logs"), mLoggerOptions(std::move(loggerOptions)), mSession(std::move(session))
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

void LogsLayer::drawToolbar(const std::vector<std::string> &sources)
{
    // Source combo: everything currently executing (Editor + background tasks).
    if(!sources.empty())
    {
        if(mSourceIndex < 0 || mSourceIndex >= static_cast<int>(sources.size()))
        {
            mSourceIndex = 0;
        }
        ImGui::SetNextItemWidth(EditorUiScale::S(200.0f));
        if(ImGui::BeginCombo("##LogSource", sources[static_cast<std::size_t>(mSourceIndex)].c_str()))
        {
            for(int i = 0; i < static_cast<int>(sources.size()); ++i)
            {
                const bool selected = i == mSourceIndex;
                if(ImGui::Selectable(sources[static_cast<std::size_t>(i)].c_str(), selected))
                {
                    mSourceIndex = i;
                }
                if(selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Log source: Editor or a running background task");
        }
        ImGui::SameLine();
    }

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

void LogsLayer::drawList(const std::string &categoryFilter)
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
            if(!categoryFilter.empty() && entry.category != categoryFilter)
            {
                continue;
            }
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

void LogsLayer::drawTaskView(const EditorBackgroundTask &task)
{
    ImVec4 stateColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    const char *stateLabel = "Running";
    if(task.state == EditorBackgroundTaskState::Succeeded)
    {
        stateColor = ImVec4(0.35f, 0.78f, 0.45f, 1.0f);
        stateLabel = "Succeeded";
    }
    else if(task.state == EditorBackgroundTaskState::Failed)
    {
        stateColor = ImVec4(0.92f, 0.38f, 0.38f, 1.0f);
        stateLabel = "Failed";
    }

    ImGui::TextUnformatted(task.title.c_str());
    ImGui::SameLine();
    ImGui::TextColored(stateColor, "%s", stateLabel);
    if(!task.detail.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", task.detail.c_str());
    }

    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, stateColor);
    if(task.state == EditorBackgroundTaskState::Running && !task.determinate)
    {
        ImGui::ProgressBar(task.progress, ImVec2(-1.0f, 0.0f), "");
    }
    else
    {
        char overlay[16] {};
        if(task.state == EditorBackgroundTaskState::Running && task.determinate)
        {
            std::snprintf(overlay, sizeof(overlay), "%.0f%%", task.progress * 100.0f);
        }
        else if(task.state == EditorBackgroundTaskState::Succeeded)
        {
            std::snprintf(overlay, sizeof(overlay), "Done");
        }
        else if(task.state == EditorBackgroundTaskState::Failed)
        {
            std::snprintf(overlay, sizeof(overlay), "Failed");
        }
        ImGui::ProgressBar(task.progress, ImVec2(-1.0f, 0.0f),
                           overlay[0] != '\0' ? overlay : "");
    }
    ImGui::PopStyleColor();

    const float availHeight = ImGui::GetContentRegionAvail().y;
    const float logHeight =
        availHeight > EditorUiScale::S(28.0f) ? availHeight - EditorUiScale::S(28.0f) : availHeight;
    if(ImGui::BeginChild("##TaskLog", ImVec2(0, logHeight), true,
                         ImGuiWindowFlags_HorizontalScrollbar))
    {
        if(task.logTail.empty())
        {
            ImGui::TextDisabled("No output yet.");
        }
        else
        {
            ImGui::TextUnformatted(task.logTail.c_str());
            if(mAutoScroll && task.state == EditorBackgroundTaskState::Running)
            {
                ImGui::SetScrollHereY(1.0f);
            }
        }
    }
    ImGui::EndChild();

    if(!task.logTail.empty() && ImGui::SmallButton("Copy log"))
    {
        ImGui::SetClipboardText(task.logTail.c_str());
    }
}

void LogsLayer::onGui()
{
    const auto windowId = EditorDock::WindowId("Logs");
    if(!ImGui::Begin(windowId.c_str()))
    {
        ImGui::End();
        return;
    }

    // Collect sources: Editor + every background task currently known.
    std::vector<EditorBackgroundTask> tasks;
    if(mSession)
    {
        tasks = mSession->GetBackgroundTasks();
    }
    std::vector<std::string> sources;
    sources.reserve(tasks.size() + 1);
    sources.emplace_back("Editor");
    for(const auto &task : tasks)
    {
        sources.push_back(task.title.empty() ? task.id : task.title);
    }
    if(mSourceIndex >= static_cast<int>(sources.size()))
    {
        mSourceIndex = 0;
    }

    drawToolbar(sources);

    // Background task selected: show its own log view with full feedback.
    if(mSourceIndex > 0 &&
       static_cast<std::size_t>(mSourceIndex - 1) < tasks.size())
    {
        ImGui::Separator();
        drawTaskView(tasks[static_cast<std::size_t>(mSourceIndex - 1)]);
        ImGui::End();
        return;
    }
    mSourceIndex = 0;

    // Editor logs: second combo filters by category (who is executing).
    std::vector<std::string> categories;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        categories.reserve(mEntries.size());
        for(const auto &entry : mEntries)
        {
            categories.push_back(entry.category);
        }
    }
    std::sort(categories.begin(), categories.end());
    categories.erase(std::unique(categories.begin(), categories.end()), categories.end());

    std::string categoryFilter;
    if(!categories.empty())
    {
        if(mCategoryIndex < 0 || mCategoryIndex > static_cast<int>(categories.size()))
        {
            mCategoryIndex = 0;
        }
        ImGui::SetNextItemWidth(EditorUiScale::S(200.0f));
        if(ImGui::BeginCombo("##LogCategory", mCategoryIndex == 0
                                                  ? "All sources"
                                                  : categories[static_cast<std::size_t>(
                                                        mCategoryIndex - 1)]
                                                        .c_str()))
        {
            if(ImGui::Selectable("All sources", mCategoryIndex == 0))
            {
                mCategoryIndex = 0;
            }
            if(mCategoryIndex == 0)
            {
                ImGui::SetItemDefaultFocus();
            }
            for(int i = 0; i < static_cast<int>(categories.size()); ++i)
            {
                const bool selected = mCategoryIndex == i + 1;
                if(ImGui::Selectable(categories[static_cast<std::size_t>(i)].c_str(),
                                     selected))
                {
                    mCategoryIndex = i + 1;
                }
                if(selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Filter by what is executing (log category)");
        }
        if(mCategoryIndex > 0 &&
           static_cast<std::size_t>(mCategoryIndex - 1) < categories.size())
        {
            categoryFilter = categories[static_cast<std::size_t>(mCategoryIndex - 1)];
        }
    }
    else
    {
        mCategoryIndex = 0;
    }

    ImGui::Separator();
    drawList(categoryFilter);

    ImGui::End();
}
