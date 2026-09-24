// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace terminal {

// Reads repo-root secrets.json and returns the "mboum" string.
// Throws std::runtime_error if the file is missing, invalid, or the key is empty.
[[nodiscard]] std::string loadMboumApiKey(const std::filesystem::path& secrets_path);

// Reads one optional string key (for example "openfigi"). nullopt when the key is
// absent or empty. Throws when the file is missing or invalid, or the value is not a string.
[[nodiscard]] std::optional<std::string> loadOptionalSecret(const std::filesystem::path& secrets_path,
                                                            std::string_view key);

}  // namespace terminal
