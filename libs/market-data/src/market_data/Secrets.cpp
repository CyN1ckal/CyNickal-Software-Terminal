// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "market_data/Secrets.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>
#include <string>

namespace terminal {

namespace {

[[nodiscard]] nlohmann::json readSecrets(const std::filesystem::path& secrets_path)
{
    std::ifstream in(secrets_path);
    if (!in)
    {
        throw std::runtime_error("cannot open secrets file: " + secrets_path.string());
    }
    try
    {
        return nlohmann::json::parse(in);
    }
    catch (const nlohmann::json::parse_error& ex)
    {
        throw std::runtime_error(std::string("invalid secrets.json: ") + ex.what());
    }
}

}  // namespace

std::string loadMboumApiKey(const std::filesystem::path& secrets_path)
{
    const nlohmann::json root = readSecrets(secrets_path);
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

std::optional<std::string> loadOptionalSecret(const std::filesystem::path& secrets_path, std::string_view key)
{
    const nlohmann::json root = readSecrets(secrets_path);
    if (!root.is_object())
    {
        throw std::runtime_error("secrets.json is not a JSON object");
    }
    const auto it = root.find(std::string(key));
    if (it == root.end() || it->is_null())
    {
        return std::nullopt;
    }
    if (!it->is_string())
    {
        throw std::runtime_error("JSON key \"" + std::string(key) + "\" is not a string");
    }
    std::string value = it->get<std::string>();
    if (value.empty())
    {
        return std::nullopt;
    }
    return value;
}

}  // namespace terminal
