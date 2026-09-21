#pragma once

#include <filesystem>
#include <string>

namespace terminal {

// Reads repo-root secrets.json and returns the "mboum" string.
// Throws std::runtime_error if the file is missing, invalid, or the key is empty.
[[nodiscard]] std::string loadMboumApiKey(const std::filesystem::path& secrets_path);

}  // namespace terminal
