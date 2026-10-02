// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "ui/AppearanceSettings.h"

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

// One theme's tokens. Stratum Dark and Light come from kPalDark and kPalLight in
// CyNickal-Software-Monorepo shared/GUI/StratumPalette.h. The website generator reads
// that header, so do not invent hex values for them. High Contrast is derived from
// Stratum Dark in paletteFor.
struct Palette
{
    // Surfaces, darkest (dark themes) or brightest (light) first.
    ImVec4 bg0;    // wells, title, fields
    ImVec4 bg1;    // window body
    ImVec4 bg2;    // cards, child panels
    ImVec4 bg3;    // hover / active
    ImVec4 line;   // hairline
    ImVec4 line2;  // stronger hairline

    ImVec4 text;        // body
    ImVec4 text_dim;    // labels
    ImVec4 text_faint;  // disabled, placeholders

    // Stratum's `amber` field is the warning hue, not the text color.
    ImVec4 accent;
    ImVec4 warn;
    ImVec4 danger;
    ImVec4 ok;

    // Gains and losses. Follows the market color setting, not the status hues.
    ImVec4 up;
    ImVec4 down;

    // Stratum accbgAlpha: the selection and header wash.
    float accent_wash_alpha;
};

[[nodiscard]] Palette paletteFor(ThemeChoice theme, MarketColors market) noexcept;
[[nodiscard]] const Palette& palette() noexcept;

// Tokens of the active palette.
[[nodiscard]] inline const ImVec4& bg0() noexcept { return palette().bg0; }
[[nodiscard]] inline const ImVec4& bg1() noexcept { return palette().bg1; }
[[nodiscard]] inline const ImVec4& bg2() noexcept { return palette().bg2; }
[[nodiscard]] inline const ImVec4& bg3() noexcept { return palette().bg3; }
[[nodiscard]] inline const ImVec4& line() noexcept { return palette().line; }
[[nodiscard]] inline const ImVec4& line2() noexcept { return palette().line2; }
[[nodiscard]] inline const ImVec4& text() noexcept { return palette().text; }
[[nodiscard]] inline const ImVec4& textDim() noexcept { return palette().text_dim; }
[[nodiscard]] inline const ImVec4& textFaint() noexcept { return palette().text_faint; }
[[nodiscard]] inline const ImVec4& accent() noexcept { return palette().accent; }
[[nodiscard]] inline const ImVec4& warn() noexcept { return palette().warn; }
[[nodiscard]] inline const ImVec4& danger() noexcept { return palette().danger; }
[[nodiscard]] inline const ImVec4& ok() noexcept { return palette().ok; }

[[nodiscard]] inline ImVec4 accentWash() noexcept { return WithAlpha(accent(), palette().accent_wash_alpha); }
[[nodiscard]] inline ImVec4 accentHover() noexcept { return Mix(accent(), FromRgb(0xFF, 0xFF, 0xFF), 0.18f); }
[[nodiscard]] inline ImVec4 accentPressed() noexcept { return Mix(accent(), bg0(), 0.22f); }

// Workstation roles.
[[nodiscard]] inline const ImVec4& canvas() noexcept { return bg0(); }
[[nodiscard]] inline const ImVec4& panel() noexcept { return bg2(); }
[[nodiscard]] inline const ImVec4& field() noexcept { return bg0(); }
[[nodiscard]] inline const ImVec4& hairline() noexcept { return line(); }
[[nodiscard]] inline const ImVec4& muted() noexcept { return textDim(); }
[[nodiscard]] inline const ImVec4& up() noexcept { return palette().up; }
[[nodiscard]] inline const ImVec4& down() noexcept { return palette().down; }
[[nodiscard]] inline const ImVec4& go() noexcept { return accent(); }
[[nodiscard]] inline const ImVec4& cancel() noexcept { return danger(); }

// Rebuilds the ImGui and ImPlot styles from the settings. Call between frames.
// dpi_scale is the monitor content scale; the font setting multiplies it.
void apply(const AppearanceSettings& settings, float dpi_scale);
void LoadFonts(ImGuiIO& io);

[[nodiscard]] ImFont* sansFont() noexcept;
[[nodiscard]] ImFont* monoFont() noexcept;

// Display scale times the font zoom. Layout constants written for a 13 px font at
// 100% scaling go through px() so they grow with the text.
[[nodiscard]] float scale() noexcept;
[[nodiscard]] inline float px(float value) noexcept { return value * scale(); }

}  // namespace terminal::Theme
