// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include <filesystem>
#include <system_error>

namespace terminal {

// A directory that cannot be read is not a repo root: the walk carries on, and
// findRepoRoot falls back to the working directory instead of throwing out of the probe.
[[nodiscard]] inline bool isSuperprojectRoot(const std::filesystem::path& dir)
{
    std::error_code ec;
    const bool has_build_file = std::filesystem::is_regular_file(dir / "CMakeLists.txt", ec);
    return has_build_file && std::filesystem::is_directory(dir / "libs" / "market-data", ec);
}

[[nodiscard]] inline std::filesystem::path findRepoRoot()
{
    auto dir = std::filesystem::current_path();
    for (int i = 0; i < 16; ++i)
    {
        if (isSuperprojectRoot(dir))
        {
            return dir;
        }
        if (!dir.has_parent_path() || dir == dir.parent_path())
        {
            break;
        }
        dir = dir.parent_path();
    }
    return std::filesystem::current_path();
}

[[nodiscard]] inline std::filesystem::path defaultMarketDataDbPath()
{
    return findRepoRoot() / "data" / "market-data.sqlite";
}

[[nodiscard]] inline std::filesystem::path defaultSecretsPath()
{
    return findRepoRoot() / "secrets.json";
}

}  // namespace terminal
