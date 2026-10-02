// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "chart/CChartbookFile.h"
#include "ui/AppearanceSettings.h"

#include <filesystem>
#include <fstream>

TEST_CASE("appearance tokens round trip and unknown tokens are rejected")
{
    for (int index = 0; index < static_cast<int>(terminal::ThemeChoice::Count); ++index)
    {
        const auto theme = static_cast<terminal::ThemeChoice>(index);
        terminal::ThemeChoice parsed{terminal::ThemeChoice::Count};
        REQUIRE(terminal::themeFromToken(terminal::themeToken(theme), parsed));
        CHECK(parsed == theme);
    }
    for (int index = 0; index < static_cast<int>(terminal::MarketColors::Count); ++index)
    {
        const auto market = static_cast<terminal::MarketColors>(index);
        terminal::MarketColors parsed{terminal::MarketColors::Count};
        REQUIRE(terminal::marketColorsFromToken(terminal::marketColorsToken(market), parsed));
        CHECK(parsed == market);
    }
    for (int index = 0; index < static_cast<int>(terminal::Density::Count); ++index)
    {
        const auto density = static_cast<terminal::Density>(index);
        terminal::Density parsed{terminal::Density::Count};
        REQUIRE(terminal::densityFromToken(terminal::densityToken(density), parsed));
        CHECK(parsed == density);
    }

    terminal::ThemeChoice theme = terminal::ThemeChoice::Light;
    CHECK_FALSE(terminal::themeFromToken("solarized", theme));
    CHECK(theme == terminal::ThemeChoice::Light);
    CHECK(terminal::clampFontPx(1) == terminal::kFontPxMin);
    CHECK(terminal::clampFontPx(99) == terminal::kFontPxMax);
}

TEST_CASE("appearance and startup sections of terminal.json keep each other")
{
    const std::filesystem::path directory = std::filesystem::temp_directory_path() / "terminal-appearance-tests";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const std::filesystem::path path = directory / "terminal.json";

    // Missing file: defaults, not an error.
    const terminal::AppearanceLoadResult missing = terminal::loadAppearanceSettings(path);
    REQUIRE(missing.ok);
    CHECK(missing.settings == terminal::AppearanceSettings{});

    terminal::AppearanceSettings look;
    look.theme = terminal::ThemeChoice::HighContrast;
    look.market = terminal::MarketColors::BlueOrange;
    look.density = terminal::Density::Compact;
    look.font_px = 16;
    look.frame_stats = true;
    REQUIRE(terminal::saveAppearanceSettings(path, look).empty());

    // A file first written by the appearance saver still loads as startup settings.
    const terminal::StartupLoadResult empty_startup = terminal::loadStartupSettings(path);
    REQUIRE(empty_startup.ok);
    CHECK(empty_startup.settings.open_on_startup.empty());

    terminal::StartupSettings startup;
    startup.open_on_startup = {"data/chartbooks/a.chartbook.json"};
    REQUIRE(terminal::saveStartupSettings(path, startup).empty());

    const terminal::AppearanceLoadResult kept = terminal::loadAppearanceSettings(path);
    REQUIRE(kept.ok);
    CHECK(kept.settings == look);

    look.font_px = 12;
    REQUIRE(terminal::saveAppearanceSettings(path, look).empty());
    const terminal::StartupLoadResult startup_kept = terminal::loadStartupSettings(path);
    REQUIRE(startup_kept.ok);
    REQUIRE(startup_kept.settings.open_on_startup.size() == 1);
    CHECK(startup_kept.settings.open_on_startup[0] == "data/chartbooks/a.chartbook.json");
    CHECK(terminal::loadAppearanceSettings(path).settings.font_px == 12);

    std::filesystem::remove_all(directory);
}

TEST_CASE("appearance fields that do not parse keep their defaults")
{
    const std::filesystem::path directory = std::filesystem::temp_directory_path() / "terminal-appearance-lenient";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const std::filesystem::path path = directory / "terminal.json";
    {
        std::ofstream out(path);
        out << R"({"version":1,"open_on_startup":[],)"
               R"("appearance":{"theme":"neon","market_colors":"blue-orange","density":7,"font_px":400}})";
    }
    const terminal::AppearanceLoadResult loaded = terminal::loadAppearanceSettings(path);
    REQUIRE(loaded.ok);
    CHECK(loaded.settings.theme == terminal::ThemeChoice::StratumDark);
    CHECK(loaded.settings.market == terminal::MarketColors::BlueOrange);
    CHECK(loaded.settings.density == terminal::Density::Standard);
    CHECK(loaded.settings.font_px == terminal::kFontPxMax);
    std::filesystem::remove_all(directory);
}
