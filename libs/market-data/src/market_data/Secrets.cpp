#include "market_data/Secrets.h"

#include <cctype>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace myapp {
namespace {

void skipWs(std::string_view& text)
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0)
    {
        text.remove_prefix(1);
    }
}

}  // namespace

std::string jsonObjectString(std::string_view json, std::string_view key)
{
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto pos = json.find(needle);
    if (pos == std::string_view::npos)
    {
        throw std::runtime_error("JSON object missing \"" + std::string(key) + "\"");
    }
    std::string_view rest = json.substr(pos + needle.size());
    skipWs(rest);
    if (rest.empty() || rest.front() != ':')
    {
        throw std::runtime_error("JSON key \"" + std::string(key) + "\" is not followed by a value");
    }
    rest.remove_prefix(1);
    skipWs(rest);
    if (rest.empty() || rest.front() != '"')
    {
        throw std::runtime_error("JSON key \"" + std::string(key) + "\" is not a string");
    }
    rest.remove_prefix(1);
    std::string out;
    bool escape = false;
    for (const char c : rest)
    {
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
    throw std::runtime_error("unterminated JSON string for \"" + std::string(key) + "\"");
}

std::string loadMboumApiKey(const std::filesystem::path& secrets_path)
{
    std::ifstream in(secrets_path);
    if (!in)
    {
        throw std::runtime_error("cannot open secrets file: " + secrets_path.string());
    }
    const std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::string key = jsonObjectString(json, "mboum");
    if (key.empty())
    {
        throw std::runtime_error("secrets.json \"mboum\" is empty");
    }
    return key;
}

}  // namespace myapp
