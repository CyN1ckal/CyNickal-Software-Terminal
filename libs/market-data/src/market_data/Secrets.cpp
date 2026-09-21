#include "market_data/Secrets.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>
#include <string>

namespace myapp {

std::string loadMboumApiKey(const std::filesystem::path& secrets_path)
{
    std::ifstream in(secrets_path);
    if (!in)
    {
        throw std::runtime_error("cannot open secrets file: " + secrets_path.string());
    }

    nlohmann::json root;
    try
    {
        root = nlohmann::json::parse(in);
    }
    catch (const nlohmann::json::parse_error& ex)
    {
        throw std::runtime_error(std::string("invalid secrets.json: ") + ex.what());
    }

    if (!root.is_object() || !root.contains("mboum"))
    {
        throw std::runtime_error("JSON object missing \"mboum\"");
    }
    const auto& mboum = root.at("mboum");
    if (!mboum.is_string())
    {
        throw std::runtime_error("JSON key \"mboum\" is not a string");
    }
    std::string key = mboum.get<std::string>();
    if (key.empty())
    {
        throw std::runtime_error("secrets.json \"mboum\" is empty");
    }
    return key;
}

}  // namespace myapp
