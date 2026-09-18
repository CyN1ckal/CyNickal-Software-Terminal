#include "Theme.h"

#include <array>
#include <cstdio>

namespace myapp::Theme {
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

ImFont* loadFirstAvailable(ImGuiIO& io, const std::array<const char*, 4>& paths, float size_px,
                           const char* debug_name, const ImWchar* ranges)
{
    for (const char* path : paths)
    {
        if (path == nullptr)
        {
            continue;
        }

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

void ApplyBloombergStyle(ImGuiStyle& style)
{
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
    colors[ImGuiCol_Text] = kAmber;
    colors[ImGuiCol_TextDisabled] = kMuted;
    colors[ImGuiCol_WindowBg] = kCanvas;
    colors[ImGuiCol_ChildBg] = kPanel;
    colors[ImGuiCol_PopupBg] = kCanvas;
    colors[ImGuiCol_Border] = kHairline;
    colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_FrameBg] = kChrome;
    colors[ImGuiCol_FrameBgHovered] = kChromeHover;
    colors[ImGuiCol_FrameBgActive] = kFrameActive;
    colors[ImGuiCol_TitleBg] = kCanvas;
    colors[ImGuiCol_TitleBgActive] = kTitleActive;
    colors[ImGuiCol_TitleBgCollapsed] = kCanvas;
    colors[ImGuiCol_MenuBarBg] = kChrome;
    colors[ImGuiCol_ScrollbarBg] = kCanvas;
    colors[ImGuiCol_ScrollbarGrab] = kChromeHover;
    colors[ImGuiCol_ScrollbarGrabHovered] = kButtonActive;
    colors[ImGuiCol_ScrollbarGrabActive] = kHairline;
    colors[ImGuiCol_CheckMark] = kAmber;
    colors[ImGuiCol_CheckboxSelectedBg] = kChromeHover;
    colors[ImGuiCol_SliderGrab] = kAmber;
    colors[ImGuiCol_SliderGrabActive] = kHighlight;
    colors[ImGuiCol_Button] = kChrome;
    colors[ImGuiCol_ButtonHovered] = kChromeHover;
    colors[ImGuiCol_ButtonActive] = kButtonActive;
    colors[ImGuiCol_Header] = kChrome;
    colors[ImGuiCol_HeaderHovered] = kChromeHover;
    colors[ImGuiCol_HeaderActive] = kTitleActive;
    colors[ImGuiCol_Separator] = kHairline;
    colors[ImGuiCol_SeparatorHovered] = kHairline;
    colors[ImGuiCol_SeparatorActive] = kAmber;
    colors[ImGuiCol_ResizeGrip] = WithAlpha(kHairline, 0.40f);
    colors[ImGuiCol_ResizeGripHovered] = WithAlpha(kAmber, 0.67f);
    colors[ImGuiCol_ResizeGripActive] = kAmber;
    colors[ImGuiCol_InputTextCursor] = kAmber;
    colors[ImGuiCol_TabHovered] = WithAlpha(kHighlight, 0.35f);
    colors[ImGuiCol_Tab] = kChrome;
    colors[ImGuiCol_TabSelected] = kTabSelected;
    colors[ImGuiCol_TabSelectedOverline] = kHighlight;
    colors[ImGuiCol_TabDimmed] = kCanvas;
    colors[ImGuiCol_TabDimmedSelected] = kTitleActive;
    colors[ImGuiCol_TabDimmedSelectedOverline] = WithAlpha(kHighlight, 0.40f);
    colors[ImGuiCol_DockingPreview] = WithAlpha(kAmber, 0.25f);
    colors[ImGuiCol_DockingEmptyBg] = kCanvas;
    colors[ImGuiCol_PlotLines] = kAmber;
    colors[ImGuiCol_PlotLinesHovered] = kSeries;
    colors[ImGuiCol_PlotHistogram] = kUp;
    colors[ImGuiCol_PlotHistogramHovered] = kHighlight;
    colors[ImGuiCol_TableHeaderBg] = kChrome;
    colors[ImGuiCol_TableBorderStrong] = kHairline;
    colors[ImGuiCol_TableBorderLight] = kTableBorderLight;
    colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_TableRowBgAlt] = kTableRowAlt;
    colors[ImGuiCol_TextLink] = kSeries;
    colors[ImGuiCol_TextSelectedBg] = WithAlpha(kAmber, 0.35f);
    colors[ImGuiCol_TreeLines] = kHairline;
    colors[ImGuiCol_DragDropTarget] = WithAlpha(kAmber, 0.90f);
    colors[ImGuiCol_DragDropTargetBg] = WithAlpha(kAmber, 0.15f);
    colors[ImGuiCol_UnsavedMarker] = kHighlight;
    colors[ImGuiCol_NavCursor] = kSeries;
    colors[ImGuiCol_NavWindowingHighlight] = WithAlpha(kAmber, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = WithAlpha(kCanvas, 0.60f);
    colors[ImGuiCol_ModalWindowDimBg] = WithAlpha(kCanvas, 0.60f);
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
    const std::array<const char*, 4> sans_paths{
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
    };
    const std::array<const char*, 4> mono_paths{
        "/usr/share/fonts/truetype/noto/NotoSansMono-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        nullptr,
    };

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

}  // namespace myapp::Theme

