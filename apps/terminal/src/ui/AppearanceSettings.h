// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include <cstdint>
#include <string_view>

namespace terminal {

// User-chosen look of the terminal. Plain data with no ImGui, so it is saved and
// tested with the rest of the settings code. Theme applies it to the ImGui style.

enum class ThemeChoice : std::uint8_t
{
    StratumDark = 0,
    HighContrast,
    Light,
    Count,
};

// Colors for gains and losses: candles, P&L, signed changes.
enum class MarketColors : std::uint8_t
{
    GreenRed = 0,
    // Distinguishable with red-green color blindness (deuteranopia, protanopia).
    BlueOrange,
    // East Asian convention: red rises, green falls.
    RedGreen,
    Count,
};

enum class Density : std::uint8_t
{
    Compact = 0,
    Standard,
    Comfortable,
    Count,
};

inline constexpr int kFontPxMin = 10;
inline constexpr int kFontPxMax = 22;
inline constexpr int kFontPxDefault = 13;

struct AppearanceSettings
{
    ThemeChoice theme{ThemeChoice::StratumDark};
    MarketColors market{MarketColors::GreenRed};
    Density density{Density::Standard};
    // Body text height in pixels at 100% display scaling. The monitor's DPI scale multiplies it.
    int font_px{kFontPxDefault};
    // Frame time readout on the status rail.
    bool frame_stats{false};

    bool operator==(const AppearanceSettings&) const = default;
};

[[nodiscard]] int clampFontPx(int px) noexcept;

// Stable tokens for the settings file, and labels for the GUI.
[[nodiscard]] std::string_view themeToken(ThemeChoice theme) noexcept;
[[nodiscard]] std::string_view marketColorsToken(MarketColors market) noexcept;
[[nodiscard]] std::string_view densityToken(Density density) noexcept;
[[nodiscard]] const char* themeLabel(ThemeChoice theme) noexcept;
[[nodiscard]] const char* marketColorsLabel(MarketColors market) noexcept;
[[nodiscard]] const char* densityLabel(Density density) noexcept;

// False when the token is unknown; out is left unchanged.
[[nodiscard]] bool themeFromToken(std::string_view token, ThemeChoice& out) noexcept;
[[nodiscard]] bool marketColorsFromToken(std::string_view token, MarketColors& out) noexcept;
[[nodiscard]] bool densityFromToken(std::string_view token, Density& out) noexcept;

}  // namespace terminal
