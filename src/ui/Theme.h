#pragma once

#include "imgui.h"

namespace myapp::Theme {

constexpr ImVec4 FromRgb(int r, int g, int b, float a = 1.0f)
{
    return {static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
            static_cast<float>(b) / 255.0f, a};
}

constexpr ImVec4 WithAlpha(const ImVec4& color, float alpha)
{
    return {color.x, color.y, color.z, alpha};
}

// Surfaces
constexpr ImVec4 kCanvas = FromRgb(0x00, 0x00, 0x00);
constexpr ImVec4 kPanel = FromRgb(0x0A, 0x0A, 0x0A);
constexpr ImVec4 kChrome = FromRgb(0x1C, 0x1C, 0x1C);
constexpr ImVec4 kChromeHover = FromRgb(0x2C, 0x2C, 0x2C);
constexpr ImVec4 kHairline = FromRgb(0x3C, 0x3C, 0x3C);

// Text
constexpr ImVec4 kAmber = FromRgb(0xFF, 0xA0, 0x28);
constexpr ImVec4 kInk = FromRgb(0xFF, 0xFF, 0xFF);
constexpr ImVec4 kMuted = FromRgb(0x8C, 0x8C, 0x8C);
constexpr ImVec4 kHighlight = FromRgb(0xFC, 0xBC, 0x14);

// Semantic (data)
constexpr ImVec4 kUp = FromRgb(0x04, 0x84, 0x1C);
constexpr ImVec4 kDown = FromRgb(0xA4, 0x1C, 0x2C);
constexpr ImVec4 kSeries = FromRgb(0x4D, 0xC7, 0xF9);
constexpr ImVec4 kHeatUp = FromRgb(0x04, 0x4C, 0x0C);
constexpr ImVec4 kHeatDown = FromRgb(0x7C, 0x0C, 0x24);

// Action
constexpr ImVec4 kGo = FromRgb(0x1C, 0x8C, 0x28);
constexpr ImVec4 kCancel = FromRgb(0xC4, 0x14, 0x28);
constexpr ImVec4 kSector = FromRgb(0xF0, 0xC4, 0x00);
constexpr ImVec4 kPanelKey = FromRgb(0x2A, 0x7F, 0xD4);

// Chrome accents not in the named token table
constexpr ImVec4 kTitleActive = FromRgb(0x3A, 0x14, 0x08);
constexpr ImVec4 kTabSelected = FromRgb(0x5A, 0x1C, 0x20);
constexpr ImVec4 kFrameActive = FromRgb(0x36, 0x36, 0x36);
constexpr ImVec4 kButtonActive = FromRgb(0x3A, 0x3A, 0x3A);
constexpr ImVec4 kTableBorderLight = FromRgb(0x2A, 0x2A, 0x2A);
constexpr ImVec4 kTableRowAlt = FromRgb(0x0C, 0x0C, 0x0C);

void ApplyBloombergStyle(ImGuiStyle& style);
void LoadFonts(ImGuiIO& io);

[[nodiscard]] ImFont* sansFont() noexcept;
[[nodiscard]] ImFont* monoFont() noexcept;

}  // namespace myapp::Theme

