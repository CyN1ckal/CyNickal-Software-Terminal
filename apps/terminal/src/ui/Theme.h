// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "imgui.h"

namespace terminal::Theme {

constexpr ImVec4 FromRgb(int r, int g, int b, float a = 1.0f)
{
    return {static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
            static_cast<float>(b) / 255.0f, a};
}

constexpr ImVec4 WithAlpha(const ImVec4& color, float alpha)
{
    return {color.x, color.y, color.z, alpha};
}

constexpr ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t)
{
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t,
            a.w + (b.w - a.w) * t};
}

// Stratum dark palette. Byte-identical to kPalDark in
// CyNickal-Software-Monorepo shared/GUI/StratumPalette.h.
// The website generator reads that header; do not invent a second set of hex values.

// Surfaces
constexpr ImVec4 kBg0 = FromRgb(0x0D, 0x11, 0x16);  // #0d1116 wells, title, fields
constexpr ImVec4 kBg1 = FromRgb(0x12, 0x17, 0x1E);  // #12171e window body
constexpr ImVec4 kBg2 = FromRgb(0x17, 0x1D, 0x26);  // #171d26 cards, child panels
constexpr ImVec4 kBg3 = FromRgb(0x1E, 0x25, 0x30);  // #1e2530 hover / active
constexpr ImVec4 kLine = FromRgb(0x26, 0x2E, 0x3A);  // #262e3a hairline
constexpr ImVec4 kLine2 = FromRgb(0x31, 0x38, 0x48);  // #313848 stronger hairline

// Text
constexpr ImVec4 kText = FromRgb(0xCC, 0xD3, 0xDD);      // #ccd3dd body
constexpr ImVec4 kTextDim = FromRgb(0x82, 0x8C, 0x9B);   // #828c9b labels
constexpr ImVec4 kTextFaint = FromRgb(0x58, 0x62, 0x73); // #586273 disabled

// Accent and status. Stratum's `amber` field is the warning hue, not the text color.
constexpr ImVec4 kAccent = FromRgb(0x6F, 0x97, 0xC9);  // #6f97c9
constexpr ImVec4 kWarn = FromRgb(0xC0, 0x85, 0x52);    // #c08552
constexpr ImVec4 kDanger = FromRgb(0xB5, 0x54, 0x4E);  // #b5544e
constexpr ImVec4 kOk = FromRgb(0x5F, 0x8A, 0x63);      // #5f8a63

constexpr float kAccentWashAlpha = 0.16f;
constexpr ImVec4 kAccentWash = WithAlpha(kAccent, kAccentWashAlpha);
constexpr ImVec4 kAccentHover = Mix(kAccent, FromRgb(0xFF, 0xFF, 0xFF), 0.18f);
constexpr ImVec4 kAccentPressed = Mix(kAccent, kBg0, 0.22f);

// Workstation roles. Same hex as the tokens above.
constexpr ImVec4 kCanvas = kBg0;
constexpr ImVec4 kPanel = kBg2;
constexpr ImVec4 kField = kBg0;
constexpr ImVec4 kHairline = kLine;
constexpr ImVec4 kMuted = kTextDim;
constexpr ImVec4 kUp = kOk;
constexpr ImVec4 kDown = kDanger;
constexpr ImVec4 kGo = kAccent;
constexpr ImVec4 kCancel = kDanger;

void ApplyStratumStyle(ImGuiStyle& style);
void LoadFonts(ImGuiIO& io);

[[nodiscard]] ImFont* sansFont() noexcept;
[[nodiscard]] ImFont* monoFont() noexcept;

}  // namespace terminal::Theme
