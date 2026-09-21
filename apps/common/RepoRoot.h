#pragma once

#include <filesystem>

namespace myapp {

[[nodiscard]] inline bool isSuperprojectRoot(const std::filesystem::path& dir)
{
    return std::filesystem::is_regular_file(dir / "CMakeLists.txt") &&
           std::filesystem::is_directory(dir / "libs" / "market-data");
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

}  // namespace myapp
