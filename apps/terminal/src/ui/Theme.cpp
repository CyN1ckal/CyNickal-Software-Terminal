// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "ui/Theme.h"

#include <array>
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

}  // namespace

void ApplyStratumStyle(ImGuiStyle& style)
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

    style.WindowPadding = ImVec2(6.0f, 4.0f);
    style.FramePadding = ImVec2(6.0f, 3.0f);
    style.ItemSpacing = ImVec2(6.0f, 4.0f);
    style.ItemInnerSpacing = ImVec2(4.0f, 3.0f);
    style.CellPadding = ImVec2(4.0f, 2.0f);
    style.IndentSpacing = 12.0f;
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 8.0f;

    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    style.DisplaySafeAreaPadding = ImVec2(0.0f, 0.0f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = kText;
    colors[ImGuiCol_TextDisabled] = kTextFaint;
    colors[ImGuiCol_WindowBg] = kBg1;
    colors[ImGuiCol_ChildBg] = kBg2;
    colors[ImGuiCol_PopupBg] = kBg3;
    colors[ImGuiCol_Border] = kLine;
    colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_FrameBg] = kBg0;
    colors[ImGuiCol_FrameBgHovered] = kBg3;
    colors[ImGuiCol_FrameBgActive] = kBg3;
    colors[ImGuiCol_TitleBg] = kBg0;
    colors[ImGuiCol_TitleBgActive] = kBg0;
    colors[ImGuiCol_TitleBgCollapsed] = kBg0;
    colors[ImGuiCol_MenuBarBg] = kBg1;
    colors[ImGuiCol_ScrollbarBg] = kBg0;
    colors[ImGuiCol_ScrollbarGrab] = kLine2;
    colors[ImGuiCol_ScrollbarGrabHovered] = kTextDim;
    colors[ImGuiCol_ScrollbarGrabActive] = kAccent;
    colors[ImGuiCol_CheckMark] = kBg0;
    colors[ImGuiCol_CheckboxSelectedBg] = kAccent;
    colors[ImGuiCol_SliderGrab] = kAccent;
    colors[ImGuiCol_SliderGrabActive] = kText;
    colors[ImGuiCol_Button] = kBg2;
    colors[ImGuiCol_ButtonHovered] = kBg3;
    colors[ImGuiCol_ButtonActive] = kLine2;
    colors[ImGuiCol_Header] = kAccentWash;
    colors[ImGuiCol_HeaderHovered] = WithAlpha(kAccent, 0.24f);
    colors[ImGuiCol_HeaderActive] = WithAlpha(kAccent, 0.32f);
    colors[ImGuiCol_Separator] = kLine;
    colors[ImGuiCol_SeparatorHovered] = kLine2;
    colors[ImGuiCol_SeparatorActive] = kAccent;
    colors[ImGuiCol_ResizeGrip] = WithAlpha(kLine, 0.40f);
    colors[ImGuiCol_ResizeGripHovered] = kAccent;
    colors[ImGuiCol_ResizeGripActive] = kAccent;
    colors[ImGuiCol_InputTextCursor] = kAccent;
    colors[ImGuiCol_TabHovered] = kBg3;
    colors[ImGuiCol_Tab] = kBg0;
    colors[ImGuiCol_TabSelected] = kBg1;
    colors[ImGuiCol_TabSelectedOverline] = kAccent;
    colors[ImGuiCol_TabDimmed] = kBg0;
    colors[ImGuiCol_TabDimmedSelected] = kBg1;
    colors[ImGuiCol_TabDimmedSelectedOverline] = WithAlpha(kAccent, 0.40f);
    colors[ImGuiCol_DockingPreview] = WithAlpha(kAccent, 0.35f);
    colors[ImGuiCol_DockingEmptyBg] = kBg0;
    colors[ImGuiCol_PlotLines] = kAccent;
    colors[ImGuiCol_PlotLinesHovered] = kText;
    colors[ImGuiCol_PlotHistogram] = kOk;
    colors[ImGuiCol_PlotHistogramHovered] = kAccent;
    colors[ImGuiCol_TableHeaderBg] = kBg2;
    colors[ImGuiCol_TableBorderStrong] = kLine;
    colors[ImGuiCol_TableBorderLight] = kBg3;
    colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_TableRowBgAlt] = kBg0;
    colors[ImGuiCol_TextLink] = kAccent;
    colors[ImGuiCol_TextSelectedBg] = WithAlpha(kAccent, 0.28f);
    colors[ImGuiCol_TreeLines] = kLine;
    colors[ImGuiCol_DragDropTarget] = WithAlpha(kAccent, 0.90f);
    colors[ImGuiCol_DragDropTargetBg] = WithAlpha(kAccent, 0.15f);
    colors[ImGuiCol_UnsavedMarker] = kWarn;
    colors[ImGuiCol_NavCursor] = kAccent;
    colors[ImGuiCol_NavWindowingHighlight] = WithAlpha(kAccent, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = WithAlpha(kBg0, 0.60f);
    colors[ImGuiCol_ModalWindowDimBg] = WithAlpha(kBg0, 0.60f);
}

void LoadFonts(ImGuiIO& io)
{
    static const ImWchar kRanges[] = {
        0x0020, 0x00FF,  // Basic Latin + Latin-1
        0x2013, 0x2014,  // en-dash, em-dash
        0x20AC, 0x20AC,  // euro
        0x2190, 0x2195,  // arrows
        0x2212, 0x2212,  // minus
        0,
    };

    constexpr float kSizePx = 13.0f;
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

