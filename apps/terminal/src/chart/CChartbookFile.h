// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "chart/CChartbookDocument.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace terminal {

struct ChartbookLoadResult
{
    bool ok{false};
    std::string error;
    CChartbookDocument document{};
};

struct StartupSettings
{
    std::vector<std::string> open_on_startup;
};

struct StartupLoadResult
{
    bool ok{false};
    std::string error;
    StartupSettings settings{};
};

struct StartupOpenResult
{
    struct Opened
    {
        std::filesystem::path path;
        CChartbookDocument document{};
    };

    std::vector<Opened> books;
    std::vector<std::string> errors;
};

[[nodiscard]] std::filesystem::path chartbooksDirectory();
[[nodiscard]] std::filesystem::path defaultTerminalSettingsPath();

[[nodiscard]] bool isSafeChartbookStem(std::string_view stem);
// Empty when the stem is not a single safe path component.
[[nodiscard]] std::filesystem::path chartbookPathForStem(std::string_view stem);
[[nodiscard]] std::string chartbookStemFromPath(const std::filesystem::path& path);

[[nodiscard]] std::string pathForStorage(const std::filesystem::path& path);
[[nodiscard]] std::filesystem::path pathFromStorage(std::string_view stored);
[[nodiscard]] bool chartbookPathsEqual(const std::filesystem::path& left, const std::filesystem::path& right);

[[nodiscard]] std::string chartbookToJson(const CChartbookDocument& document);
[[nodiscard]] ChartbookLoadResult chartbookFromJson(std::string_view text);

// Writes path.tmp, flushes it, then replaces path. An existing file is copied to path.bak
// only after that replacement succeeds. Returns an empty string on success.
[[nodiscard]] std::string saveChartbook(const std::filesystem::path& path, const CChartbookDocument& document);
[[nodiscard]] ChartbookLoadResult loadChartbook(const std::filesystem::path& path);

[[nodiscard]] StartupLoadResult loadStartupSettings(const std::filesystem::path& path);
[[nodiscard]] std::string saveStartupSettings(const std::filesystem::path& path, const StartupSettings& settings);
[[nodiscard]] StartupOpenResult openStartupChartbooks(const StartupSettings& settings);

}  // namespace terminal
