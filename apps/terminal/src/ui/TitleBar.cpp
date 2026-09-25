// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "ui/TitleBar.h"

#include "platform/window/FrameChrome.h"
#include "platform/window/Window.h"
#include "ui/ChartbookHost.h"
#include "ui/InventoryPanel.h"
#include "ui/Theme.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace terminal {
namespace {

// Shorter than StratumChrome::kTitleBarH (38). The terminal keeps the menus on
// this same bar, so it does not need a second strip. Scaled with the font DPI.
constexpr float kTitleBarHeight = 30.0f;
constexpr float kCaptionButtonWidth = 46.0f;
constexpr float kFrameBorder = 8.0f;
constexpr float kTitleTextPad = 12.0f;
constexpr float kTitleMenuGap = 18.0f;
constexpr int kCaptionButtonCount = 3;

enum class CaptionButton : unsigned char { Minimize, Maximize, Close };

[[nodiscard]] float uiScale()
{
    const float dpi = ImGui::GetStyle().FontScaleDpi;
    return dpi > 0.0f ? dpi : 1.0f;
}

[[nodiscard]] ImVec2 titleBarFramePadding()
{
    const float font = ImGui::GetFontSize();
    const float height = std::max(kTitleBarHeight * uiScale(), font + 2.0f);
    const float pad_y = std::max(0.0f, (height - font) * 0.5f);
    return {ImGui::GetStyle().FramePadding.x, pad_y};
}

void releaseLeftButton()
{
    ImGuiIO& io = ImGui::GetIO();
    io.MouseDown[ImGuiMouseButton_Left] = false;
    io.MouseClicked[ImGuiMouseButton_Left] = false;
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
}

[[nodiscard]] bool isFrameWindow(const ImGuiWindow* window)
{
    return window != nullptr && window->Name != nullptr && std::strncmp(window->Name, "##frame_", 8) == 0;
}

[[nodiscard]] const char* frameWindowId(FrameEdge edge)
{
    switch (edge)
    {
    case FrameEdge::Left:
        return "##frame_left";
    case FrameEdge::Right:
        return "##frame_right";
    case FrameEdge::Top:
        return "##frame_top";
    case FrameEdge::Bottom:
        return "##frame_bottom";
    case FrameEdge::TopLeft:
        return "##frame_top_left";
    case FrameEdge::TopRight:
        return "##frame_top_right";
    case FrameEdge::BottomLeft:
        return "##frame_bottom_left";
    case FrameEdge::BottomRight:
        return "##frame_bottom_right";
    case FrameEdge::None:
        return "##frame_none";
    }
    return "##frame_none";
}

[[nodiscard]] ImGuiMouseCursor cursorForEdge(FrameEdge edge)
{
    switch (edge)
    {
    case FrameEdge::Left:
    case FrameEdge::Right:
        return ImGuiMouseCursor_ResizeEW;
    case FrameEdge::Top:
    case FrameEdge::Bottom:
        return ImGuiMouseCursor_ResizeNS;
    case FrameEdge::TopLeft:
    case FrameEdge::BottomRight:
        return ImGuiMouseCursor_ResizeNWSE;
    case FrameEdge::TopRight:
    case FrameEdge::BottomLeft:
        return ImGuiMouseCursor_ResizeNESW;
    case FrameEdge::None:
        return ImGuiMouseCursor_Arrow;
    }
    return ImGuiMouseCursor_Arrow;
}

void liftPopupsAboveFrame()
{
    ImGuiContext& context = *ImGui::GetCurrentContext();
    constexpr int kMaxPopups = 32;
    // BringWindowToDisplayFront takes a mutable window.
    ImGuiWindow* popups[kMaxPopups] = {}; // NOLINT(misc-const-correctness)
    int count = 0;
    for (int i = 0; i < context.Windows.Size; ++i)
    {
        ImGuiWindow* window = context.Windows[i];
        if (window == nullptr || count >= kMaxPopups)
        {
            continue;
        }
        const int popup_flags = ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip;
        if ((window->Flags & popup_flags) != 0)
        {
            popups[count] = window;
            ++count;
        }
    }
    for (int i = 0; i < count; ++i)
    {
        ImGui::BringWindowToDisplayFront(popups[i]);
    }
}

// Wait until the pointer actually moves so a double-click can maximize
// before the window manager's move grab swallows the second click.
void restoreFocus(ImGuiWindow* previous)
{
    if (previous != nullptr)
    {
        ImGui::FocusWindow(previous);
    }
}

void handleCaptionDrag(Window& window, ImVec2 drag_min, ImVec2 drag_max, ImGuiWindow* previous_focus)
{
    static bool armed = false;

    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const bool over = mouse.x >= drag_min.x && mouse.x < drag_max.x && mouse.y >= drag_min.y &&
                      mouse.y < drag_max.y && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered();
    if (over && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        armed = false;
        window.toggleMaximize();
        restoreFocus(previous_focus);
        return;
    }
    if (over && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        armed = true;
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        armed = false;
    }
    if (armed && ImGui::IsMouseDragging(ImGuiMouseButton_Left, -1.0f))
    {
        armed = false;
        window.beginMove();
        releaseLeftButton();
        restoreFocus(previous_focus);
    }
}

// Returns the screen x where the menus should start.
[[nodiscard]] float drawBrand(ImDrawList* draw_list, ImVec2 origin, float height, float scale, float buttons_left)
{
    const float text_x = origin.x + (kTitleTextPad * scale);
    const float text_y = origin.y + ((height - ImGui::GetTextLineHeight()) * 0.5f);
    const float text_w = ImGui::CalcTextSize(kWindowTitle).x;
    const float clip_right = buttons_left - (8.0f * scale);
    const float text_right = std::min(text_x + text_w, clip_right);
    if (text_right > text_x)
    {
        ImGui::PushClipRect(ImVec2(text_x, origin.y), ImVec2(text_right, origin.y + height), true);
        draw_list->AddText(ImVec2(text_x, text_y), ImGui::GetColorU32(Theme::kTextDim), kWindowTitle);
        ImGui::PopClipRect();
    }
    return text_right + (kTitleMenuGap * scale);
}

void drawCaptionGlyph(ImDrawList* draw_list, CaptionButton button, ImVec2 center, float scale, ImU32 color,
                      bool maximized)
{
    const float extent = 5.0f * scale;
    const float thickness = std::max(1.0f, 1.2f * scale);
    if (button == CaptionButton::Minimize)
    {
        draw_list->AddLine(ImVec2(center.x - extent, center.y), ImVec2(center.x + extent, center.y), color,
                           thickness);
        return;
    }
    if (button == CaptionButton::Close)
    {
        draw_list->AddLine(ImVec2(center.x - extent, center.y - extent),
                           ImVec2(center.x + extent, center.y + extent), color, thickness);
        draw_list->AddLine(ImVec2(center.x + extent, center.y - extent),
                           ImVec2(center.x - extent, center.y + extent), color, thickness);
        return;
    }
    if (maximized)
    {
        const float shift = 2.0f * scale;
        draw_list->AddRect(ImVec2(center.x - extent + (shift * 2.0f), center.y - extent),
                           ImVec2(center.x + extent, center.y + extent - (shift * 2.0f)), color, 0.0f, 0, thickness);
        draw_list->AddRect(ImVec2(center.x - extent, center.y - extent + (shift * 2.0f)),
                           ImVec2(center.x + extent - (shift * 2.0f), center.y + extent), color, 0.0f, 0, thickness);
        return;
    }
    draw_list->AddRect(ImVec2(center.x - extent, center.y - extent), ImVec2(center.x + extent, center.y + extent),
                       color, 0.0f, 0, thickness);
}

[[nodiscard]] float captionButtonsLeft(ImVec2 origin, float width, float scale)
{
    const float cluster = kCaptionButtonWidth * scale * static_cast<float>(kCaptionButtonCount);
    return origin.x + width - cluster;
}

void drawCaptionButtons(ImDrawList* draw_list, ImVec2 origin, ImVec2 size, float scale, Window& window,
                        ImGuiWindow* previous_focus)
{
    const float button_w = kCaptionButtonWidth * scale;
    const float buttons_left = captionButtonsLeft(origin, size.x, scale);
    const bool maximized = window.isMaximized();
    const CaptionButton buttons[kCaptionButtonCount] = {CaptionButton::Minimize, CaptionButton::Maximize,
                                                        CaptionButton::Close,};
    const char* ids[kCaptionButtonCount] = {"##min", "##max", "##close"};

    ImGui::PushItemFlag(ImGuiItemFlags_NoNavDefaultFocus, true);
    for (int i = 0; i < kCaptionButtonCount; ++i)
    {
        const ImVec2 bmin(buttons_left + (button_w * static_cast<float>(i)), origin.y);
        const ImVec2 bmax(bmin.x + button_w, origin.y + size.y);
        ImGui::SetCursorScreenPos(bmin);
        ImGui::PushID(i);
        const bool pressed = ImGui::InvisibleButton(ids[i], ImVec2(button_w, size.y));
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();

        if (hovered)
        {
            const ImVec4 fill = buttons[i] == CaptionButton::Close ? Theme::kDanger : Theme::kBg3;
            draw_list->AddRectFilled(bmin, bmax, ImGui::GetColorU32(fill));
        }
        const ImVec4 glyph =
            (hovered && buttons[i] == CaptionButton::Close) ? Theme::kBg0 : Theme::kTextDim;
        const ImVec2 center((bmin.x + bmax.x) * 0.5f, (bmin.y + bmax.y) * 0.5f);
        drawCaptionGlyph(draw_list, buttons[i], center, scale, ImGui::GetColorU32(glyph), maximized);

        if (!pressed)
        {
            continue;
        }
        if (buttons[i] == CaptionButton::Minimize)
        {
            window.minimize();
        }
        else if (buttons[i] == CaptionButton::Maximize)
        {
            window.toggleMaximize();
            restoreFocus(previous_focus);
        }
        else
        {
            window.requestClose();
        }
    }
    ImGui::PopItemFlag();
}

[[nodiscard]] ImGuiWindow* drawResizeBand(const FrameBorderRect& band, ImVec2 viewport_pos, ImGuiID viewport_id,
                                          Window& window, ImGuiWindow* restore_focus)
{
    const ImVec2 pos(viewport_pos.x + band.x, viewport_pos.y + band.y);
    const ImVec2 size(band.width, band.height);
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGui::SetNextWindowViewport(viewport_id);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground;
    const bool open = ImGui::Begin(frameWindowId(band.edge), nullptr, flags);
    ImGui::PopStyleVar(3);
    if (!open)
    {
        ImGui::End();
        return nullptr;
    }

    ImGuiWindow* frame_window = ImGui::GetCurrentWindow();
    ImGui::SetCursorScreenPos(pos);
    ImGui::InvisibleButton("hit", size);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetMouseCursor(cursorForEdge(band.edge));
    }
    if (ImGui::IsItemActivated())
    {
        window.beginResize(band.edge);
        releaseLeftButton();
        if (restore_focus != nullptr)
        {
            ImGui::FocusWindow(restore_focus);
        }
    }
    ImGui::End();
    return frame_window;
}

}  // namespace

bool beginTitleMenu(const char* label)
{
    if (!ImGui::BeginMenu(label))
    {
        return false;
    }
    // The caption's frame padding would otherwise stretch every dropdown row.
    ImGui::PopStyleVar();
    return true;
}

void endTitleMenu()
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, titleBarFramePadding());
    ImGui::EndMenu();
}

void drawTitleBar(Window& window, ChartbookHost& books, InventoryPanel& inventory)
{
    ImGuiWindow* previous_focus = ImGui::GetCurrentContext()->NavWindow;
    if (previous_focus != nullptr && previous_focus->Name != nullptr &&
        std::strcmp(previous_focus->Name, "##TitleBar") == 0)
    {
        previous_focus = nullptr;
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float scale = uiScale();
    const float font = ImGui::GetFontSize();
    const float height = std::max(kTitleBarHeight * scale, font + 2.0f);
    const ImVec2 frame_padding = titleBarFramePadding();
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoFocusOnAppearing;

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, frame_padding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::kBg0);
    const bool open = ImGui::BeginViewportSideBar("##TitleBar", viewport, ImGuiDir_Up, height, flags);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    if (!open)
    {
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    const float buttons_left = captionButtonsLeft(origin, size.x, scale);

    // The sidebar is only as tall as its menu bar, so the content clip is empty
    // until BeginMenuBar replaces it. Buttons drawn earlier never appear.
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, Theme::kBg0);
    if (ImGui::BeginMenuBar())
    {
        const float menu_x = drawBrand(draw_list, origin, size.y, scale, buttons_left);
        ImGui::SetCursorScreenPos(ImVec2(menu_x, origin.y));
        books.drawTitleMenus(inventory, buttons_left);
        drawCaptionButtons(draw_list, origin, size, scale, window, previous_focus);
        draw_list->AddLine(ImVec2(origin.x, origin.y + size.y - 1.0f),
                           ImVec2(origin.x + size.x, origin.y + size.y - 1.0f), ImGui::GetColorU32(Theme::kLine));
        ImGui::EndMenuBar();
    }
    ImGui::PopStyleColor();
    handleCaptionDrag(window, origin, ImVec2(buttons_left, origin.y + size.y), previous_focus);
    ImGui::End();
    ImGui::PopStyleVar();
}

void drawWindowResizeBorders(Window& window)
{
    if (window.isMaximized())
    {
        return;
    }

    ImGuiViewport const* viewport = ImGui::GetMainViewport();
    const float border = kFrameBorder * uiScale();
    const FrameBorders bands = frameBorderRects(viewport->Size.x, viewport->Size.y, border);
    ImGuiWindow* restore_focus = ImGui::GetCurrentContext()->NavWindow;
    if (isFrameWindow(restore_focus) || restore_focus == ImGui::FindWindowByName("##TitleBar"))
    {
        restore_focus = nullptr;
    }

    // BringWindowToDisplayFront takes a mutable window.
    ImGuiWindow* frames[8] = {}; // NOLINT(misc-const-correctness)
    int frame_count = 0;
    for (int i = 0; i < bands.count; ++i)
    {
        ImGuiWindow* frame = drawResizeBand(bands.rects[static_cast<std::size_t>(i)], viewport->Pos, viewport->ID,
                                            window, restore_focus);
        if (frame != nullptr && std::cmp_less(frame_count, sizeof(frames) / sizeof(frames[0])))
        {
            frames[frame_count] = frame;
            ++frame_count;
        }
    }
    for (int i = 0; i < frame_count; ++i)
    {
        ImGui::BringWindowToDisplayFront(frames[i]);
    }
    liftPopupsAboveFrame();
}

}  // namespace terminal
