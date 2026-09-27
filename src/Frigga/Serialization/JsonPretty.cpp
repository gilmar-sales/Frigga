#include <Frigga/Serialization/JsonPretty.hpp>

#include <string>

namespace FRIGGA_NAMESPACE
{
    std::string PrettifyJson(std::string_view minified, int indentStep)
    {
        if(indentStep <= 0)
        {
            indentStep = 2;
        }

        std::string out;
        out.reserve(minified.size() + minified.size() / 4 + 16);

        auto appendIndent = [&](int depth) {
            out.append(static_cast<std::size_t>(depth * indentStep), ' ');
        };

        // Next non-whitespace character at or after pos (minified input has
        // none, but callers may pass already-formatted JSON).
        auto peekNext = [&](std::size_t pos) -> char {
            for(; pos < minified.size(); ++pos)
            {
                const char ch = minified[pos];
                if(ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r')
                {
                    return ch;
                }
            }
            return '\0';
        };

        int depth    = 0;
        bool inStr   = false;
        bool escaped = false;
        bool ok      = true;

        for(std::size_t i = 0; i < minified.size(); ++i)
        {
            const char ch = minified[i];
            if(inStr)
            {
                out.push_back(ch);
                if(escaped)
                {
                    escaped = false;
                }
                else if(ch == '\\')
                {
                    escaped = true;
                }
                else if(ch == '"')
                {
                    inStr = false;
                }
                continue;
            }

            switch(ch)
            {
                case '"':
                    inStr = true;
                    out.push_back(ch);
                    break;
                case '{':
                case '[':
                {
                    const char closer = ch == '{' ? '}' : ']';
                    // Collapse empty objects/arrays to {} / [].
                    if(peekNext(i + 1) == closer)
                    {
                        out.push_back(ch);
                        out.push_back(closer);
                        // Skip ahead past the closer (for loop's ++i moves beyond).
                        std::size_t j = i + 1;
                        while(j < minified.size() && (minified[j] == ' ' || minified[j] == '\t' ||
                                                      minified[j] == '\n' || minified[j] == '\r'))
                        {
                            ++j;
                        }
                        i = j; // j is the closer; ++i steps past it
                        break;
                    }
                    out.push_back(ch);
                    out.push_back('\n');
                    ++depth;
                    appendIndent(depth);
                    break;
                }
                case '}':
                case ']':
                    out.push_back('\n');
                    --depth;
                    if(depth < 0)
                    {
                        ok    = false;
                        depth = 0;
                    }
                    appendIndent(depth);
                    out.push_back(ch);
                    break;
                case ',':
                    out.push_back(ch);
                    out.push_back('\n');
                    appendIndent(depth);
                    break;
                case ':':
                    out.append(": ");
                    break;
                case ' ':
                case '\t':
                case '\n':
                case '\r':
                    break;
                default:
                    out.push_back(ch);
                    break;
            }
        }

        if(!ok || inStr || escaped || depth != 0)
        {
            return std::string(minified);
        }
        return out;
    }

} // namespace FRIGGA_NAMESPACE
