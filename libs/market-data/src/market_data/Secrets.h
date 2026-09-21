#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace myapp {

// Reads repo-root secrets.json and returns the "mboum" string.
// Throws std::runtime_error if the file is missing, invalid, or the key is empty.
[[nodiscard]] std::string loadMboumApiKey(const std::filesystem::path& secrets_path);

// Extracts a JSON object string field. Quotes are unescaped for \\ and \".
[[nodiscard]] std::string jsonObjectString(std::string_view json, std::string_view key);

}  // namespace myapp
