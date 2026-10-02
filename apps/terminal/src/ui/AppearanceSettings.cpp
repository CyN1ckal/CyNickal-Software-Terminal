// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "ui/AppearanceSettings.h"

#include <algorithm>
#include <array>

namespace terminal {
namespace {

struct Named
{
    std::string_view token;
    const char* label;
};

constexpr std::array<Named, static_cast<std::size_t>(ThemeChoice::Count)> kThemes{{
    {.token = "stratum-dark", .label = "Stratum Dark"},
    {.token = "high-contrast", .label = "High Contrast"},
    {.token = "light", .label = "Light"},
},};

constexpr std::array<Named, static_cast<std::size_t>(MarketColors::Count)> kMarkets{{
    {.token = "green-red", .label = "Green up / Red down"},
    {.token = "blue-orange", .label = "Blue up / Orange down (color-blind safe)"},
    {.token = "red-green", .label = "Red up / Green down"},
},};

constexpr std::array<Named, static_cast<std::size_t>(Density::Count)> kDensities{{
    {.token = "compact", .label = "Compact"},
    {.token = "standard", .label = "Standard"},
    {.token = "comfortable", .label = "Comfortable"},
},};

template <typename Enum, std::size_t N>
[[nodiscard]] const Named& named(const std::array<Named, N>& table, Enum value) noexcept
{
    const auto index = static_cast<std::size_t>(value);
    return index < N ? table[index] : table[0];
}

template <typename Enum, std::size_t N>
[[nodiscard]] bool fromToken(const std::array<Named, N>& table, std::string_view token, Enum& out) noexcept
{
    for (std::size_t index = 0; index < N; ++index)
    {
        if (table[index].token == token)
        {
            out = static_cast<Enum>(index);
            return true;
        }
    }
    return false;
}

}  // namespace

int clampFontPx(int px) noexcept
{
    return std::clamp(px, kFontPxMin, kFontPxMax);
}

std::string_view themeToken(ThemeChoice theme) noexcept
{
    return named(kThemes, theme).token;
}

std::string_view marketColorsToken(MarketColors market) noexcept
{
    return named(kMarkets, market).token;
}

std::string_view densityToken(Density density) noexcept
{
    return named(kDensities, density).token;
}

const char* themeLabel(ThemeChoice theme) noexcept
{
    return named(kThemes, theme).label;
}

const char* marketColorsLabel(MarketColors market) noexcept
{
    return named(kMarkets, market).label;
}

const char* densityLabel(Density density) noexcept
{
    return named(kDensities, density).label;
}

bool themeFromToken(std::string_view token, ThemeChoice& out) noexcept
{
    return fromToken(kThemes, token, out);
}

bool marketColorsFromToken(std::string_view token, MarketColors& out) noexcept
{
    return fromToken(kMarkets, token, out);
}

bool densityFromToken(std::string_view token, Density& out) noexcept
{
    return fromToken(kDensities, token, out);
}

}  // namespace terminal
