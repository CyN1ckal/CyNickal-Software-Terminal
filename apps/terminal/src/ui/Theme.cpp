// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "ui/Theme.h"

#include "implot.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>

namespace terminal::Theme {
namespace {

struct LoadedFonts
{
    ImFont* sans = nullptr;
    ImFont* mono = nullptr;
};

LoadedFonts& loadedFonts() noexcept
{
    static LoadedFonts fonts;
    return fonts;
}

Palette& activePalette() noexcept
{
    static Palette active = paletteFor(ThemeChoice::StratumDark, MarketColors::GreenRed);
    return active;
}

float& activeScale() noexcept
{
    static float active = 1.0f;
    return active;
}

template <std::size_t N>
ImFont* loadFirstAvailable(ImGuiIO& io, const std::array<const char*, N>& paths, float size_px,
                           const char* debug_name, const ImWchar* ranges)
{
    for (const char* path : paths)
    {
        ImFontConfig config;
        config.SizePixels = size_px;
        std::snprintf(config.Name, sizeof(config.Name), "%s", debug_name);
        if (ImFont* font = io.Fonts->AddFontFromFileTTF(path, size_px, &config, ranges))
        {
            return font;
        }
    }
    return nullptr;
}

// Same hue, a new HSL lightness, and saturation raised to at least min_saturation.
[[nodiscard]] ImVec4 withLightness(const ImVec4& color, float lightness, float min_saturation) noexcept
{
    const float hi = std::max({color.x, color.y, color.z});
    const float lo = std::min({color.x, color.y, color.z});
    float hue = 0.0f;
    float sat = 0.0f;
    if (hi > lo)
    {
        const float d = hi - lo;
        sat = (hi + lo) > 1.0f ? d / (2.0f - hi - lo) : d / (hi + lo);
        if (hi == color.x)
        {
            hue = ((color.y - color.z) / d) + (color.y < color.z ? 6.0f : 0.0f);
        }
        else if (hi == color.y)
        {
            hue = ((color.z - color.x) / d) + 2.0f;
        }
        else
        {
            hue = ((color.x - color.y) / d) + 4.0f;
        }
        hue /= 6.0f;
    }
    sat = std::max(sat, min_saturation);
    const float q = lightness < 0.5f ? lightness * (1.0f + sat) : lightness + sat - (lightness * sat);
    const float p = (2.0f * lightness) - q;
    const auto channel = [p, q](float t) {
        t -= std::floor(t);
        if (t < 1.0f / 6.0f)
        {
            return p + ((q - p) * 6.0f * t);
        }
        if (t < 0.5f)
        {
            return q;
        }
        if (t < 2.0f / 3.0f)
        {
            return p + ((q - p) * ((2.0f / 3.0f) - t) * 6.0f);
        }
        return p;
    };
    return {channel(hue + (1.0f / 3.0f)), channel(hue), channel(hue - (1.0f / 3.0f)), color.w};
}

void applyShape(ImGuiStyle& style, Density density)
{
    // Square and tight. Stratum's product shell uses 7–8 px radii and looser
    // padding; a data workstation stays flat so more rows fit.
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.TabRounding = 0.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.TabBorderSize = 0.0f;

    // Standard is the original workstation spacing. Compact trades padding for
    // rows; Comfortable gives larger pointer targets.
    switch (density)
    {
    case Density::Compact:
        style.WindowPadding = ImVec2(4.0f, 3.0f);
        style.FramePadding = ImVec2(4.0f, 1.0f);
        style.ItemSpacing = ImVec2(4.0f, 2.0f);
        style.ItemInnerSpacing = ImVec2(3.0f, 2.0f);
        style.CellPadding = ImVec2(3.0f, 1.0f);
        style.ScrollbarSize = 10.0f;
        break;
    case Density::Comfortable:
        style.WindowPadding = ImVec2(10.0f, 8.0f);
        style.FramePadding = ImVec2(8.0f, 5.0f);
        style.ItemSpacing = ImVec2(8.0f, 6.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
        style.CellPadding = ImVec2(6.0f, 4.0f);
        style.ScrollbarSize = 14.0f;
        break;
    case Density::Standard:
    case Density::Count:
        style.WindowPadding = ImVec2(6.0f, 4.0f);
        style.FramePadding = ImVec2(6.0f, 3.0f);
        style.ItemSpacing = ImVec2(6.0f, 4.0f);
        style.ItemInnerSpacing = ImVec2(4.0f, 3.0f);
        style.CellPadding = ImVec2(4.0f, 2.0f);
        style.ScrollbarSize = 12.0f;
        break;
    }
    style.IndentSpacing = 12.0f;
    style.GrabMinSize = 8.0f;

    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    style.DisplaySafeAreaPadding = ImVec2(0.0f, 0.0f);
}

void applyColors(ImGuiStyle& style, const Palette& p, bool light)
{
    const ImVec4 clear(0.0f, 0.0f, 0.0f, 0.0f);
    // A dark scrim reads as "behind" on both light and dark surfaces.
    const ImVec4 scrim = light ? WithAlpha(p.text, 0.30f) : WithAlpha(p.bg0, 0.60f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = p.text;
    colors[ImGuiCol_TextDisabled] = p.text_faint;
    colors[ImGuiCol_WindowBg] = p.bg1;
    colors[ImGuiCol_ChildBg] = p.bg2;
    // Light popups sit on white so they lift off the grey body.
    colors[ImGuiCol_PopupBg] = light ? p.bg2 : p.bg3;
    colors[ImGuiCol_Border] = p.line;
    colors[ImGuiCol_BorderShadow] = clear;
    colors[ImGuiCol_FrameBg] = p.bg0;
    colors[ImGuiCol_FrameBgHovered] = p.bg3;
    colors[ImGuiCol_FrameBgActive] = p.bg3;
    colors[ImGuiCol_TitleBg] = p.bg0;
    colors[ImGuiCol_TitleBgActive] = p.bg0;
    colors[ImGuiCol_TitleBgCollapsed] = p.bg0;
    colors[ImGuiCol_MenuBarBg] = p.bg1;
    colors[ImGuiCol_ScrollbarBg] = p.bg0;
    colors[ImGuiCol_ScrollbarGrab] = p.line2;
    colors[ImGuiCol_ScrollbarGrabHovered] = p.text_dim;
    colors[ImGuiCol_ScrollbarGrabActive] = p.accent;
    colors[ImGuiCol_CheckMark] = p.bg0;
    colors[ImGuiCol_CheckboxSelectedBg] = p.accent;
    colors[ImGuiCol_SliderGrab] = p.accent;
    colors[ImGuiCol_SliderGrabActive] = p.text;
    colors[ImGuiCol_Button] = p.bg2;
    colors[ImGuiCol_ButtonHovered] = p.bg3;
    colors[ImGuiCol_ButtonActive] = p.line2;
    colors[ImGuiCol_Header] = WithAlpha(p.accent, p.accent_wash_alpha);
    colors[ImGuiCol_HeaderHovered] = WithAlpha(p.accent, 0.24f);
    colors[ImGuiCol_HeaderActive] = WithAlpha(p.accent, 0.32f);
    colors[ImGuiCol_Separator] = p.line;
    colors[ImGuiCol_SeparatorHovered] = p.line2;
    colors[ImGuiCol_SeparatorActive] = p.accent;
    colors[ImGuiCol_ResizeGrip] = WithAlpha(p.line, 0.40f);
    colors[ImGuiCol_ResizeGripHovered] = p.accent;
    colors[ImGuiCol_ResizeGripActive] = p.accent;
    colors[ImGuiCol_InputTextCursor] = p.accent;
    colors[ImGuiCol_TabHovered] = p.bg3;
    colors[ImGuiCol_Tab] = p.bg0;
    colors[ImGuiCol_TabSelected] = p.bg1;
    colors[ImGuiCol_TabSelectedOverline] = p.accent;
    colors[ImGuiCol_TabDimmed] = p.bg0;
    colors[ImGuiCol_TabDimmedSelected] = p.bg1;
    colors[ImGuiCol_TabDimmedSelectedOverline] = WithAlpha(p.accent, 0.40f);
    colors[ImGuiCol_DockingPreview] = WithAlpha(p.accent, 0.35f);
    colors[ImGuiCol_DockingEmptyBg] = p.bg0;
    colors[ImGuiCol_PlotLines] = p.accent;
    colors[ImGuiCol_PlotLinesHovered] = p.text;
    colors[ImGuiCol_PlotHistogram] = p.ok;
    colors[ImGuiCol_PlotHistogramHovered] = p.accent;
    colors[ImGuiCol_TableHeaderBg] = p.bg2;
    colors[ImGuiCol_TableBorderStrong] = p.line;
    colors[ImGuiCol_TableBorderLight] = p.bg3;
    colors[ImGuiCol_TableRowBg] = clear;
    colors[ImGuiCol_TableRowBgAlt] = p.bg0;
    colors[ImGuiCol_TextLink] = p.accent;
    colors[ImGuiCol_TextSelectedBg] = WithAlpha(p.accent, 0.28f);
    colors[ImGuiCol_TreeLines] = p.line;
    colors[ImGuiCol_DragDropTarget] = WithAlpha(p.accent, 0.90f);
    colors[ImGuiCol_DragDropTargetBg] = WithAlpha(p.accent, 0.15f);
    colors[ImGuiCol_UnsavedMarker] = p.warn;
    colors[ImGuiCol_NavCursor] = p.accent;
    colors[ImGuiCol_NavWindowingHighlight] = WithAlpha(p.accent, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = scrim;
    colors[ImGuiCol_ModalWindowDimBg] = scrim;
}

void applyPlotStyle(const Palette& p, float size_scale)
{
    ImPlotStyle& out = ImPlot::GetStyle();
    out = ImPlotStyle();
    ImPlot::StyleColorsAuto(&out);
    out.Colors[ImPlotCol_PlotBg] = p.bg0;
    out.Colors[ImPlotCol_FrameBg] = p.bg1;
    out.Colors[ImPlotCol_PlotBorder] = p.line;
    out.Colors[ImPlotCol_AxisText] = p.text_dim;
    out.Colors[ImPlotCol_AxisGrid] = p.line;
    out.Colors[ImPlotCol_Crosshairs] = p.accent;
    out.Colors[ImPlotCol_LegendBg] = WithAlpha(p.bg2, 0.92f);
    out.Colors[ImPlotCol_LegendBorder] = p.line;
    out.Colors[ImPlotCol_LegendText] = p.text;
    out.Colors[ImPlotCol_InlayText] = p.text;
    out.Colors[ImPlotCol_TitleText] = p.text;

    // ImPlot sizes are pixels, and ImGui's ScaleAllSizes does not reach them.
    const auto grow = [size_scale](ImVec2& value) {
        value.x *= size_scale;
        value.y *= size_scale;
    };
    grow(out.PlotPadding);
    grow(out.LabelPadding);
    grow(out.LegendPadding);
    grow(out.LegendInnerPadding);
    grow(out.LegendSpacing);
    grow(out.MousePosPadding);
    grow(out.AnnotationPadding);
    grow(out.PlotDefaultSize);
    grow(out.PlotMinSize);
    grow(out.MajorTickLen);
    grow(out.MinorTickLen);
    out.DigitalPadding *= size_scale;
    out.DigitalSpacing *= size_scale;
}

}  // namespace

Palette paletteFor(ThemeChoice theme, MarketColors market) noexcept
{
    // Stratum dark: kPalDark in CyNickal-Software-Monorepo shared/GUI/StratumPalette.h.
    // The monorepo has since raised txf to #7d8592; text_faint here is still #586273.
    Palette dark{};
    dark.bg0 = FromRgb(0x0D, 0x11, 0x16);         // #0d1116 wells, title, fields
    dark.bg1 = FromRgb(0x12, 0x17, 0x1E);         // #12171e window body
    dark.bg2 = FromRgb(0x17, 0x1D, 0x26);         // #171d26 cards, child panels
    dark.bg3 = FromRgb(0x1E, 0x25, 0x30);         // #1e2530 hover / active
    dark.line = FromRgb(0x26, 0x2E, 0x3A);        // #262e3a hairline
    dark.line2 = FromRgb(0x31, 0x38, 0x48);       // #313848 stronger hairline
    dark.text = FromRgb(0xCC, 0xD3, 0xDD);        // #ccd3dd body
    dark.text_dim = FromRgb(0x82, 0x8C, 0x9B);    // #828c9b labels
    dark.text_faint = FromRgb(0x58, 0x62, 0x73);  // #586273 disabled
    dark.accent = FromRgb(0x6F, 0x97, 0xC9);      // #6f97c9
    dark.warn = FromRgb(0xC0, 0x85, 0x52);        // #c08552
    dark.danger = FromRgb(0xB5, 0x54, 0x4E);      // #b5544e
    dark.ok = FromRgb(0x5F, 0x8A, 0x63);          // #5f8a63
    dark.accent_wash_alpha = 0.16f;

    Palette p = dark;
    // The color-blind-safe pair. Blue and orange differ in hue and in lightness, so
    // they stay apart under red-green color blindness.
    ImVec4 blue = FromRgb(0x5B, 0x9B, 0xD5);
    ImVec4 orange = FromRgb(0xD9, 0x89, 0x3F);
    switch (theme)
    {
    case ThemeChoice::HighContrast:
    {
        // Stratum dark pushed apart: a black base, near-white text, and the same hues
        // at higher lightness and saturation. Every text token is at least 6:1 on every
        // surface and at least 7:1 (WCAG AAA) on the window body.
        const ImVec4 white = FromRgb(0xFF, 0xFF, 0xFF);
        const ImVec4 black = FromRgb(0x00, 0x00, 0x00);
        p.bg0 = black;
        p.bg1 = Mix(dark.bg0, black, 0.4f);
        p.bg2 = dark.bg0;
        p.bg3 = dark.bg2;
        p.line = dark.line2;
        p.line2 = Mix(dark.line2, white, 0.25f);
        p.text = Mix(dark.text, white, 0.6f);
        p.text_dim = Mix(dark.text, white, 0.1f);
        p.text_faint = Mix(dark.text_dim, white, 0.3f);
        p.accent = withLightness(dark.accent, 0.72f, 0.60f);
        p.warn = withLightness(dark.warn, 0.68f, 0.60f);
        p.danger = withLightness(dark.danger, 0.68f, 0.70f);
        p.ok = withLightness(dark.ok, 0.62f, 0.45f);
        blue = withLightness(blue, 0.70f, 0.65f);
        orange = withLightness(orange, 0.64f, 0.70f);
        break;
    }
    case ThemeChoice::Light:
        // Stratum light: kPalLight in the same monorepo header.
        p.bg0 = FromRgb(0xE7, 0xEB, 0xF1);
        p.bg1 = FromRgb(0xF3, 0xF5, 0xF9);
        p.bg2 = FromRgb(0xFF, 0xFF, 0xFF);
        p.bg3 = FromRgb(0xE4, 0xE9, 0xF0);
        p.line = FromRgb(0xDD, 0xE3, 0xEA);
        p.line2 = FromRgb(0xCB, 0xD4, 0xDE);
        p.text = FromRgb(0x2A, 0x32, 0x3D);
        p.text_dim = FromRgb(0x60, 0x6B, 0x79);
        p.text_faint = FromRgb(0x63, 0x6B, 0x73);
        p.accent = FromRgb(0x3E, 0x6C, 0xA1);
        p.warn = FromRgb(0xA8, 0x6A, 0x2C);
        p.danger = FromRgb(0xA3, 0x4A, 0x44);
        p.ok = FromRgb(0x4F, 0x7A, 0x54);
        p.accent_wash_alpha = 0.12f;
        // Darker so each is at least 4.5:1 on every light surface.
        blue = FromRgb(0x2F, 0x62, 0xA0);
        orange = FromRgb(0x9A, 0x4F, 0x00);
        break;
    case ThemeChoice::StratumDark:
    case ThemeChoice::Count:
        break;
    }
    switch (market)
    {
    case MarketColors::BlueOrange:
        p.up = blue;
        p.down = orange;
        break;
    case MarketColors::RedGreen:
        p.up = p.danger;
        p.down = p.ok;
        break;
    case MarketColors::GreenRed:
    case MarketColors::Count:
        p.up = p.ok;
        p.down = p.danger;
        break;
    }
    return p;
}

const Palette& palette() noexcept
{
    return activePalette();
}

float scale() noexcept
{
    return activeScale();
}

void apply(const AppearanceSettings& settings, float dpi_scale)
{
    const Palette p = paletteFor(settings.theme, settings.market);
    activePalette() = p;
    const float zoom = static_cast<float>(clampFontPx(settings.font_px)) / static_cast<float>(kFontPxDefault);
    const float dpi = dpi_scale > 0.0f ? dpi_scale : 1.0f;
    activeScale() = dpi * zoom;

    ImGuiStyle style;
    applyShape(style, settings.density);
    applyColors(style, p, settings.theme == ThemeChoice::Light);
    style.ScaleAllSizes(dpi * zoom);
    style.FontSizeBase = static_cast<float>(kFontPxDefault);
    style.FontScaleMain = zoom;
    style.FontScaleDpi = dpi;
    ImGui::GetStyle() = style;

    applyPlotStyle(p, dpi * zoom);
}

void LoadFonts(ImGuiIO& io)
{
    static const ImWchar kRanges[] = {
        0x0020, 0x00FF,  // Basic Latin + Latin-1
        0x2013, 0x2014,  // en-dash, em-dash
        0x2022, 0x2026,  // bullet, ellipsis
        0x20AC, 0x20AC,  // euro
        0x2190, 0x2195,  // arrows
        0x2212, 0x2212,  // minus
        0x2318, 0x2318,  // command key
        0x25B2, 0x25BC,  // up and down triangles
        0,
    };

    constexpr auto kSizePx = static_cast<float>(kFontPxDefault);
#ifdef _WIN32
    const std::array<const char*, 3> sans_paths{
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/calibri.ttf",
        "C:/Windows/Fonts/arial.ttf",
    };
    const std::array<const char*, 2> mono_paths{
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/cour.ttf",
    };
#else
    const std::array<const char*, 4> sans_paths{
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
    };
    const std::array<const char*, 3> mono_paths{
        "/usr/share/fonts/truetype/noto/NotoSansMono-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
    };
#endif

    LoadedFonts& fonts = loadedFonts();
    fonts.sans = loadFirstAvailable(io, sans_paths, kSizePx, "Sans", kRanges);
    fonts.mono = loadFirstAvailable(io, mono_paths, kSizePx, "Mono", kRanges);

    if (fonts.sans != nullptr)
    {
        io.FontDefault = fonts.sans;
    }
}

ImFont* sansFont() noexcept
{
    return loadedFonts().sans;
}

ImFont* monoFont() noexcept
{
    return loadedFonts().mono;
}

}  // namespace terminal::Theme
