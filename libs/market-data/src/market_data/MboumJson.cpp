#include "market_data/MboumJson.h"

#include <cctype>
#include <charconv>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>

namespace myapp {
namespace {

struct Cursor
{
    std::string_view text;

    void skip()
    {
        while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0)
        {
            text.remove_prefix(1);
        }
    }

    [[nodiscard]] bool eat(char c)
    {
        skip();
        if (text.empty() || text.front() != c)
        {
            return false;
        }
        text.remove_prefix(1);
        return true;
    }

    [[nodiscard]] char peek()
    {
        skip();
        return text.empty() ? '\0' : text.front();
    }

    std::string parseString()
    {
        skip();
        if (text.empty() || text.front() != '"')
        {
            throw std::runtime_error("expected JSON string");
        }
        text.remove_prefix(1);
        std::string out;
        bool escape = false;
        while (!text.empty())
        {
            const char c = text.front();
            text.remove_prefix(1);
            if (escape)
            {
                out.push_back(c);
                escape = false;
                continue;
            }
            if (c == '\\')
            {
                escape = true;
                continue;
            }
            if (c == '"')
            {
                return out;
            }
            out.push_back(c);
        }
        throw std::runtime_error("unterminated JSON string");
    }

    double parseNumber()
    {
        skip();
        const auto* begin = text.data();
        const auto* end = begin + text.size();
        double value = 0.0;
        const auto [ptr, ec] = std::from_chars(begin, end, value);
        if (ec != std::errc{} || ptr == begin)
        {
            throw std::runtime_error("expected JSON number");
        }
        text.remove_prefix(static_cast<std::size_t>(ptr - begin));
        return value;
    }

    bool parseLiteralBool()
    {
        skip();
        if (text.size() >= 4 && text.substr(0, 4) == "true")
        {
            text.remove_prefix(4);
            return true;
        }
        if (text.size() >= 5 && text.substr(0, 5) == "false")
        {
            text.remove_prefix(5);
            return false;
        }
        throw std::runtime_error("expected JSON boolean");
    }

    void skipValue()
    {
        skip();
        if (text.empty())
        {
            throw std::runtime_error("expected JSON value");
        }
        const char c = text.front();
        if (c == '"')
        {
            parseString();
            return;
        }
        if (c == 't' || c == 'f')
        {
            parseLiteralBool();
            return;
        }
        if (text.size() >= 4 && text.substr(0, 4) == "null")
        {
            text.remove_prefix(4);
            return;
        }
        if (c != '{' && c != '[')
        {
            parseNumber();
            return;
        }
        int depth = 0;
        bool in_string = false;
        bool escape = false;
        while (!text.empty())
        {
            const char ch = text.front();
            text.remove_prefix(1);
            if (in_string)
            {
                if (escape)
                {
                    escape = false;
                    continue;
                }
                if (ch == '\\')
                {
                    escape = true;
                    continue;
                }
                if (ch == '"')
                {
                    in_string = false;
                }
                continue;
            }
            if (ch == '"')
            {
                in_string = true;
                continue;
            }
            if (ch == '{' || ch == '[')
            {
                ++depth;
            }
            else if (ch == '}' || ch == ']')
            {
                --depth;
                if (depth == 0)
                {
                    return;
                }
            }
        }
        throw std::runtime_error("unterminated JSON value");
    }

    template <typename Fn>
    void parseObject(const Fn& on_field)
    {
        if (!eat('{'))
        {
            throw std::runtime_error("expected JSON object");
        }
        if (eat('}'))
        {
            return;
        }
        for (;;)
        {
            const std::string key = parseString();
            if (!eat(':'))
            {
                throw std::runtime_error("expected ':' after JSON key");
            }
            on_field(key, *this);
            if (eat('}'))
            {
                return;
            }
            if (!eat(','))
            {
                throw std::runtime_error("expected comma in JSON object");
            }
        }
    }
};

[[nodiscard]] bool splitsTruthy(std::string_view text, double number, bool from_number, bool from_bool, bool bool_value)
{
    if (from_bool)
    {
        return bool_value;
    }
    if (from_number)
    {
        return number != 0.0;
    }
    return text == "1" || text == "true" || text == "True";
}

}  // namespace

MboumV3Page parseMboumV3Historical(std::string_view json)
{
    MboumV3Page page;
    Cursor cur{json};
    cur.parseObject([&](std::string_view key, Cursor& c) {
        if (key == "meta")
        {
            c.parseObject([&](std::string_view mkey, Cursor& m) {
                if (mkey == "splits")
                {
                    m.skip();
                    const char ch = m.peek();
                    if (ch == '"')
                    {
                        page.splits = splitsTruthy(m.parseString(), 0.0, false, false, false);
                    }
                    else if (ch == 't' || ch == 'f')
                    {
                        page.splits = m.parseLiteralBool();
                    }
                    else
                    {
                        page.splits = m.parseNumber() != 0.0;
                    }
                }
                else
                {
                    m.skipValue();
                }
            });
            return;
        }
        if (key == "body")
        {
            if (!c.eat('['))
            {
                c.skipValue();
                return;
            }
            if (c.eat(']'))
            {
                return;
            }
            for (;;)
            {
                MboumV3BarRow row;
                bool have_dt = false;
                c.parseObject([&](std::string_view bkey, Cursor& b) {
                    if (bkey == "datetime")
                    {
                        row.datetime = b.parseString();
                        have_dt = true;
                    }
                    else if (bkey == "open")
                    {
                        row.open = b.parseNumber();
                    }
                    else if (bkey == "high")
                    {
                        row.high = b.parseNumber();
                    }
                    else if (bkey == "low")
                    {
                        row.low = b.parseNumber();
                    }
                    else if (bkey == "close")
                    {
                        row.close = b.parseNumber();
                    }
                    else if (bkey == "volume")
                    {
                        row.volume = b.parseNumber();
                    }
                    else
                    {
                        b.skipValue();
                    }
                });
                if (have_dt)
                {
                    page.bars.push_back(std::move(row));
                }
                if (c.eat(']'))
                {
                    return;
                }
                if (!c.eat(','))
                {
                    throw std::runtime_error("expected comma in body array");
                }
            }
        }
        if (key == "message")
        {
            const std::string message = c.parseString();
            if (message.find("No historical data") != std::string::npos ||
                message.find("Failed to fetch historical data") != std::string::npos)
            {
                page.no_data = true;
            }
            return;
        }
        c.skipValue();
    });
    return page;
}

std::string mboumV3HistoricalUrl(std::string_view ticker, SessionDate session_date)
{
    const int y = session_date / 10000;
    const int m = (session_date / 100) % 100;
    const int d = session_date % 100;
    char start[32]{};
    char end[32]{};
    std::snprintf(start, sizeof(start), "%04d%02d%02d093000", y, m, d);
    std::snprintf(end, sizeof(end), "%04d%02d%02d160000", y, m, d);
    std::string url = "https://api.mboum.com/v3/markets/historical?ticker=";
    url.append(ticker);
    url += "&interval=1min&limit=400&startDate=";
    url += start;
    url += "&endDate=";
    url += end;
    // Laravel boolean rules accept 0/1, not the strings "false"/"true".
    url += "&splits=0&dividends=0&order=asc";
    return url;
}

}  // namespace myapp
