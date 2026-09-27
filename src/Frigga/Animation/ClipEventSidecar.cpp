#include <Frigga/Animation/ClipEventSidecar.hpp>
#include <Frigga/Serialization/FormatVersions.hpp>

#include <simdjson.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <fstream>
#include <sstream>
#include <system_error>

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        void AppendEscapedJson(std::string &out, std::string_view value)
        {
            for(const char ch : value)
            {
                switch(ch)
                {
                case '"':
                case '\\':
                    out.push_back('\\');
                    out.push_back(ch);
                    break;
                case '\n':
                    out += "\\n";
                    break;
                case '\r':
                    out += "\\r";
                    break;
                case '\t':
                    out += "\\t";
                    break;
                default:
                    out.push_back(ch);
                    break;
                }
            }
        }

        void AppendJsonFloat(std::string &out, float value)
        {
            if(!std::isfinite(value))
            {
                out += "0.0";
                return;
            }
            auto text = std::format("{}", value);
            if(text.find_first_of(".eEnN") == std::string::npos)
            {
                text += ".0";
            }
            out += text;
        }
    } // namespace

    std::filesystem::path
    ClipEventSidecarRelativePath(const std::filesystem::path &modelRelativePath)
    {
        const std::filesystem::path relative(modelRelativePath.generic_string());
        return relative.parent_path() /
               (relative.stem().string() + std::string(kClipEventSidecarSuffix));
    }

    bool LoadClipEventSidecar(const std::filesystem::path &absolutePath, ClipEventMap &out,
                              std::string *error)
    {
        out.clear();

        simdjson::dom::parser parser;
        simdjson::dom::element document;
        if(parser.load(absolutePath.string()).get(document) != simdjson::error_code::SUCCESS)
        {
            if(error)
            {
                *error = "Unable to parse clip event sidecar: " + absolutePath.string();
            }
            return false;
        }

        simdjson::dom::object root;
        if(document.get_object().get(root) != simdjson::error_code::SUCCESS)
        {
            if(error)
            {
                *error = "Invalid clip event sidecar JSON: " + absolutePath.string();
            }
            return false;
        }

        auto eventsValue = root["events"];
        simdjson::dom::element eventsElement;
        if(eventsValue.get(eventsElement) == simdjson::error_code::NO_SUCH_FIELD)
        {
            return true;
        }
        if(eventsValue.error() != simdjson::error_code::SUCCESS)
        {
            if(error)
            {
                *error = "Invalid clip event sidecar JSON: " + absolutePath.string();
            }
            return false;
        }

        simdjson::dom::object eventsObject;
        if(eventsElement.get_object().get(eventsObject) != simdjson::error_code::SUCCESS)
        {
            if(error)
            {
                *error = "Invalid clip event sidecar JSON: " + absolutePath.string();
            }
            return false;
        }

        for(auto [clipKey, clipValue] : eventsObject)
        {
            simdjson::dom::array entries;
            if(clipValue.get_array().get(entries) != simdjson::error_code::SUCCESS)
            {
                continue;
            }
            std::vector<fra::AnimationEvent> events;
            for(auto element : entries)
            {
                simdjson::dom::object object;
                if(element.get_object().get(object) != simdjson::error_code::SUCCESS)
                {
                    continue;
                }
                std::string_view name;
                if(object["name"].get_string().get(name) != simdjson::error_code::SUCCESS)
                {
                    continue;
                }
                double time = 0.0;
                if(object["time"].get_double().get(time) != simdjson::error_code::SUCCESS)
                {
                    std::int64_t asInt = 0;
                    if(object["time"].get_int64().get(asInt) != simdjson::error_code::SUCCESS)
                    {
                        continue;
                    }
                    time = static_cast<double>(asInt);
                }
                if(!std::isfinite(time))
                {
                    continue;
                }
                events.push_back(fra::AnimationEvent{.name    = std::string(name),
                                                     .timeSec = static_cast<float>(time)});
            }
            std::ranges::stable_sort(events, [](const fra::AnimationEvent &left,
                                                const fra::AnimationEvent &right) {
                return left.timeSec < right.timeSec;
            });
            out.emplace(std::string(clipKey), std::move(events));
        }
        return true;
    }

    bool SaveClipEventSidecar(const std::filesystem::path &absolutePath,
                              const ClipEventMap &events, std::string *error)
    {
        std::error_code ec;
        if(!absolutePath.parent_path().empty())
        {
            std::filesystem::create_directories(absolutePath.parent_path(), ec);
            if(ec)
            {
                if(error)
                {
                    *error = "Unable to create clip event sidecar directory: " + ec.message();
                }
                return false;
            }
        }

        std::string out;
        out += "{\n  \"version\": ";
        out += std::to_string(FormatVersion::ClipEventSidecar);
        out += ",\n  \"events\": {\n";
        std::size_t clipIndex = 0;
        for(const auto &[clipName, clipEvents] : events)
        {
            out += "    \"";
            AppendEscapedJson(out, clipName);
            out += "\": [";
            // Map ordering already sorts clips; keep event order stable by time.
            std::vector<const fra::AnimationEvent *> ordered;
            ordered.reserve(clipEvents.size());
            for(const auto &event : clipEvents)
            {
                ordered.push_back(&event);
            }
            std::ranges::stable_sort(ordered, [](const fra::AnimationEvent *left,
                                                 const fra::AnimationEvent *right) {
                return left->timeSec < right->timeSec;
            });
            for(std::size_t i = 0; i < ordered.size(); ++i)
            {
                if(i != 0)
                {
                    out += ", ";
                }
                out += "{\"name\":\"";
                AppendEscapedJson(out, ordered[i]->name);
                out += "\",\"time\":";
                AppendJsonFloat(out, ordered[i]->timeSec);
                out += "}";
            }
            out += "]";
            if(++clipIndex < events.size())
            {
                out += ",";
            }
            out += "\n";
        }
        out += "  }\n}\n";

        std::ofstream file(absolutePath, std::ios::binary | std::ios::trunc);
        if(!file)
        {
            if(error)
            {
                *error = "Unable to write clip event sidecar: " + absolutePath.string();
            }
            return false;
        }
        file.write(out.data(), static_cast<std::streamsize>(out.size()));
        return static_cast<bool>(file);
    }

    void NormalizeClipEvents(std::vector<fra::AnimationEvent> &events, float duration)
    {
        for(auto &event : events)
        {
            if(!std::isfinite(event.timeSec))
            {
                event.timeSec = 0.0f;
            }
            if(duration > 0.0f)
            {
                event.timeSec = std::clamp(event.timeSec, 0.0f, duration);
            }
            else if(event.timeSec < 0.0f)
            {
                event.timeSec = 0.0f;
            }
        }
        std::ranges::stable_sort(events, [](const fra::AnimationEvent &left,
                                            const fra::AnimationEvent &right) {
            return left.timeSec < right.timeSec;
        });
    }

    void ApplyClipEventOverrides(std::vector<fra::AnimationClip> &clips,
                                 const ClipEventMap &overrides)
    {
        for(auto &clip : clips)
        {
            const auto it = overrides.find(clip.name);
            if(it == overrides.end())
            {
                continue;
            }
            clip.events = it->second;
            NormalizeClipEvents(clip.events, clip.duration);
        }
    }

    ClipEventMap SnapshotClipEvents(const std::vector<fra::AnimationClip> &clips)
    {
        ClipEventMap snapshot;
        for(const auto &clip : clips)
        {
            snapshot.emplace(clip.name, clip.events);
        }
        return snapshot;
    }

} // namespace FRIGGA_NAMESPACE
