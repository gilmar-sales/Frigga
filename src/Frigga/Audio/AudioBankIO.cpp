#include <Frigga/Audio/AudioBankIO.hpp>

#include <simdjson.h>

#include <cmath>
#include <cstdint>
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

        [[nodiscard]] bool ReadFloat(const simdjson::dom::object &object, std::string_view key,
                                     float &out)
        {
            simdjson::dom::element value;
            if(object[key].get(value) != simdjson::error_code::SUCCESS)
            {
                return false;
            }
            double number = 0.0;
            if(value.get_double().get(number) == simdjson::error_code::SUCCESS)
            {
                if(std::isfinite(number))
                {
                    out = static_cast<float>(number);
                }
                return true;
            }
            std::int64_t asInt = 0;
            if(value.get_int64().get(asInt) == simdjson::error_code::SUCCESS)
            {
                out = static_cast<float>(asInt);
                return true;
            }
            return false;
        }

        [[nodiscard]] bool ReadBool(const simdjson::dom::object &object, std::string_view key,
                                    bool &out)
        {
            simdjson::dom::element value;
            if(object[key].get(value) != simdjson::error_code::SUCCESS)
            {
                return false;
            }
            bool flag = false;
            if(value.get_bool().get(flag) != simdjson::error_code::SUCCESS)
            {
                return false;
            }
            out = flag;
            return true;
        }

        [[nodiscard]] bool ReadString(const simdjson::dom::object &object, std::string_view key,
                                      std::string &out)
        {
            simdjson::dom::element value;
            if(object[key].get(value) != simdjson::error_code::SUCCESS)
            {
                return false;
            }
            std::string_view text;
            if(value.get_string().get(text) != simdjson::error_code::SUCCESS)
            {
                return false;
            }
            out.assign(text);
            return true;
        }
    } // namespace

    bool ParseAudioBankDefinition(std::string_view json, AudioBankDefinition &out, std::string *error)
    {
        out.events.clear();

        simdjson::dom::parser parser;
        simdjson::dom::element document;
        const simdjson::padded_string padded {std::string(json)};
        if(parser.parse(padded).get(document) != simdjson::error_code::SUCCESS)
        {
            if(error)
            {
                *error = "invalid audio bank JSON";
            }
            return false;
        }

        simdjson::dom::object root;
        if(document.get_object().get(root) != simdjson::error_code::SUCCESS)
        {
            if(error)
            {
                *error = "audio bank root must be a JSON object";
            }
            return false;
        }

        if(root["events"].error() == simdjson::error_code::NO_SUCH_FIELD)
        {
            return true;
        }

        simdjson::dom::array events;
        if(root["events"].get_array().get(events) != simdjson::error_code::SUCCESS)
        {
            if(error)
            {
                *error = "audio bank 'events' must be an array";
            }
            return false;
        }

        for(auto element : events)
        {
            simdjson::dom::object object;
            if(element.get_object().get(object) != simdjson::error_code::SUCCESS)
            {
                continue;
            }

            AudioBankEventDef def {};
            (void)ReadString(object, "path", def.path);
            (void)ReadString(object, "clip", def.clip);
            (void)ReadFloat(object, "volume", def.volume);
            (void)ReadFloat(object, "pitch", def.pitch);
            (void)ReadBool(object, "loop", def.loop);
            (void)ReadString(object, "bus", def.bus);
            out.events.push_back(std::move(def));
        }
        return true;
    }

    std::string SerializeAudioBankDefinition(const AudioBankDefinition &bank)
    {
        std::string out;
        out += "{\n  \"events\": [\n";
        for(std::size_t i = 0; i < bank.events.size(); ++i)
        {
            const AudioBankEventDef &event = bank.events[i];
            out += "    {\n";
            out += "      \"path\": \"";
            AppendEscapedJson(out, event.path);
            out += "\",\n";
            out += "      \"clip\": \"";
            AppendEscapedJson(out, event.clip);
            out += "\",\n";
            out += "      \"volume\": ";
            AppendJsonFloat(out, event.volume);
            out += ",\n";
            out += "      \"pitch\": ";
            AppendJsonFloat(out, event.pitch);
            out += ",\n";
            out += "      \"loop\": ";
            out += event.loop ? "true" : "false";
            out += ",\n";
            out += "      \"bus\": \"";
            AppendEscapedJson(out, event.bus);
            out += "\"\n";
            out += "    }";
            if(i + 1 < bank.events.size())
            {
                out += ",";
            }
            out += "\n";
        }
        out += "  ]\n}\n";
        return out;
    }

    bool LoadAudioBankFile(const std::filesystem::path &absolutePath, AudioBankDefinition &out,
                           std::string *error)
    {
        std::ifstream file(absolutePath, std::ios::binary);
        if(!file)
        {
            if(error)
            {
                *error = "unable to open audio bank: " + absolutePath.string();
            }
            return false;
        }
        std::ostringstream buffer;
        buffer << file.rdbuf();
        return ParseAudioBankDefinition(buffer.str(), out, error);
    }

    bool SaveAudioBankFile(const std::filesystem::path &absolutePath,
                           const AudioBankDefinition &bank, std::string *error)
    {
        std::error_code ec;
        if(!absolutePath.parent_path().empty())
        {
            std::filesystem::create_directories(absolutePath.parent_path(), ec);
            if(ec)
            {
                if(error)
                {
                    *error = "unable to create audio bank directory: " + ec.message();
                }
                return false;
            }
        }

        const std::string text = SerializeAudioBankDefinition(bank);
        std::ofstream file(absolutePath, std::ios::binary | std::ios::trunc);
        if(!file)
        {
            if(error)
            {
                *error = "unable to write audio bank: " + absolutePath.string();
            }
            return false;
        }
        file.write(text.data(), static_cast<std::streamsize>(text.size()));
        if(!file)
        {
            if(error)
            {
                *error = "failed while writing audio bank: " + absolutePath.string();
            }
            return false;
        }
        return true;
    }

} // namespace FRIGGA_NAMESPACE
