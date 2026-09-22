// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "platform/window/Window.h"

#include "platform/vulkan/VulkanUtils.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <dwmapi.h>
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_DONOTROUND
#define DWMWCP_DONOTROUND 1
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_COLOR_NONE
#define DWMWA_COLOR_NONE 0xFFFFFFFE
#endif
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
// Xlib defines None, which would rewrite FrameEdge::None.
#ifdef None
#undef None
#endif
#ifdef Bool
#undef Bool
#endif
#ifdef Status
#undef Status
#endif
#ifdef True
#undef True
#endif
#ifdef False
#undef False
#endif
#endif

namespace terminal {
namespace {

constexpr int kMinWindowWidth = 720;
constexpr int kMinWindowHeight = 480;

void glfwErrorCallback(int error, const char* description)
{
    std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

#ifdef _WIN32
// Assigned from SetWindowLongPtrW when the frame proc is installed.
WNDPROC g_original_wnd_proc = nullptr; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// Stratum caption. A light-mode caption or border reads as a white bar.
constexpr COLORREF kFrameColor = RGB(0x0D, 0x11, 0x16);

void applyMaximizedClient(HWND hwnd, NCCALCSIZE_PARAMS* params)
{
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST); // NOLINT(misc-misplaced-const)
    if (monitor == nullptr || GetMonitorInfoW(monitor, &info) == FALSE)
    {
        return;
    }

    const FramePxRect window{
        .left = params->rgrc[0].left,
        .top = params->rgrc[0].top,
        .right = params->rgrc[0].right,
        .bottom = params->rgrc[0].bottom,
    };
    const FramePxRect work{
        .left = info.rcWork.left,
        .top = info.rcWork.top,
        .right = info.rcWork.right,
        .bottom = info.rcWork.bottom,
    };
    const FramePxRect client = maximizedClientRect(window, work);
    params->rgrc[0].left = client.left;
    params->rgrc[0].top = client.top;
    params->rgrc[0].right = client.right;
    params->rgrc[0].bottom = client.bottom;
}

// Drop the thin DWM border while maximized. It is the white top edge once the
// client already fills the work area. Restored windows keep the dark border.
void applyFrameBorderColor(HWND hwnd, bool zoomed)
{
    static bool in_call = false;
    static bool have_state = false;
    static bool zoomed_state = false;
    if (in_call || (have_state && zoomed_state == zoomed))
    {
        return;
    }

    in_call = true;
    const COLORREF color = zoomed ? static_cast<COLORREF>(DWMWA_COLOR_NONE) : kFrameColor;
    const HRESULT result = DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &color, sizeof(color));
    in_call = false;
    if (SUCCEEDED(result))
    {
        have_state = true;
        zoomed_state = zoomed;
    }
}

// The client fills the window. ImGui paints the caption and the resize bands,
// then starts the sizing loop with WM_NCLBUTTONDOWN. A resize hit from this
// proc would eat that click and fight the cursor those bands set.
LRESULT CALLBACK frameWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_NCCALCSIZE:
        if (wParam != 0)
        {
            const bool zoomed = IsZoomed(hwnd) != FALSE;
            if (zoomed)
            {
                // lParam is the Win32 NCCALCSIZE_PARAMS pointer packed in an integer.
                auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam); // NOLINT(performance-no-int-to-ptr)
                applyMaximizedClient(hwnd, params);
            }
            applyFrameBorderColor(hwnd, zoomed);
            return 0;
        }
        break;
    case WM_NCHITTEST:
        return HTCLIENT;
    default:
        break;
    }

    if (g_original_wnd_proc != nullptr)
    {
        return CallWindowProcW(g_original_wnd_proc, hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

[[nodiscard]] WPARAM hitTestCode(bool moving, FrameEdge edge)
{
    if (moving || edge == FrameEdge::None)
    {
        return HTCAPTION;
    }
    switch (edge)
    {
    case FrameEdge::Left:
        return HTLEFT;
    case FrameEdge::Right:
        return HTRIGHT;
    case FrameEdge::Top:
        return HTTOP;
    case FrameEdge::Bottom:
        return HTBOTTOM;
    case FrameEdge::TopLeft:
        return HTTOPLEFT;
    case FrameEdge::TopRight:
        return HTTOPRIGHT;
    case FrameEdge::BottomLeft:
        return HTBOTTOMLEFT;
    case FrameEdge::BottomRight:
        return HTBOTTOMRIGHT;
    case FrameEdge::None:
        return HTCAPTION;
    }
    return HTCLIENT;
}
#endif

#ifdef __linux__
// EWMH _NET_WM_MOVERESIZE direction field.
[[nodiscard]] int x11MoveResizeDirection(bool moving, FrameEdge edge)
{
    if (moving)
    {
        return 8;
    }
    switch (edge)
    {
    case FrameEdge::TopLeft:
        return 0;
    case FrameEdge::Top:
        return 1;
    case FrameEdge::TopRight:
        return 2;
    case FrameEdge::Right:
        return 3;
    case FrameEdge::BottomRight:
        return 4;
    case FrameEdge::Bottom:
        return 5;
    case FrameEdge::BottomLeft:
        return 6;
    case FrameEdge::Left:
        return 7;
    case FrameEdge::None:
        return 8;
    }
    return 8;
}
#endif

}  // namespace

GlfwContext::GlfwContext()
{
    glfwSetErrorCallback(glfwErrorCallback);
    if (glfwInit() == GLFW_FALSE)
    {
        throw std::runtime_error("Failed to initialize GLFW");
    }
}

GlfwContext::~GlfwContext()
{
    glfwTerminate();
}

std::vector<const char*> GlfwContext::requiredVulkanInstanceExtensions()
{
    uint32_t count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);
    if (extensions == nullptr)
    {
        throw std::runtime_error("GLFW: Vulkan not supported");
    }

    return {extensions, extensions + count};
}

Window::Window(int width, int height, const char* title)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    if (glfwVulkanSupported() == GLFW_FALSE)
    {
        throw std::runtime_error("GLFW: Vulkan not supported");
    }

    window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    if (window_ == nullptr)
    {
        throw std::runtime_error("Failed to create GLFW window");
    }

    float x_scale = 1.0f;
    float y_scale = 1.0f;
    glfwGetWindowContentScale(window_, &x_scale, &y_scale);
    const float scale = std::max(x_scale, 1.0f);
    min_width_ = static_cast<int>(static_cast<float>(kMinWindowWidth) * scale);
    min_height_ = static_cast<int>(static_cast<float>(kMinWindowHeight) * std::max(y_scale, 1.0f));
    glfwSetWindowSizeLimits(window_, min_width_, min_height_, GLFW_DONT_CARE, GLFW_DONT_CARE);

    installBorderlessFrame();
}

Window::~Window()
{
    glfwDestroyWindow(window_);
}

bool Window::shouldClose() const noexcept
{
    return glfwWindowShouldClose(window_) != GLFW_FALSE;
}

void Window::setShouldClose(bool close) noexcept
{
    glfwSetWindowShouldClose(window_, close ? GLFW_TRUE : GLFW_FALSE);
}

bool Window::isIconified() const noexcept
{
    return glfwGetWindowAttrib(window_, GLFW_ICONIFIED) != 0;
}

bool Window::isMaximized() const noexcept
{
    return glfwGetWindowAttrib(window_, GLFW_MAXIMIZED) != 0;
}

std::pair<int, int> Window::framebufferSize() const
{
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    return {width, height};
}

void Window::requestClose() noexcept
{
    glfwSetWindowShouldClose(window_, GLFW_TRUE);
}

void Window::minimize() noexcept
{
    glfwIconifyWindow(window_);
}

void Window::toggleMaximize() noexcept
{
    if (isMaximized())
    {
        glfwRestoreWindow(window_);
    }
    else
    {
        glfwMaximizeWindow(window_);
    }
}

void Window::beginMove() noexcept
{
    if (frame_drag_.active)
    {
        return;
    }
    if (beginNativeDrag(true, FrameEdge::None))
    {
        return;
    }
    beginManualDrag(true, FrameEdge::None);
}

void Window::beginResize(FrameEdge edge) noexcept
{
    if (edge == FrameEdge::None || frame_drag_.active || isMaximized())
    {
        return;
    }
    if (beginNativeDrag(false, edge))
    {
        return;
    }
    beginManualDrag(false, edge);
}

void Window::pumpFrameDrag() noexcept
{
    if (!frame_drag_.active || window_ == nullptr)
    {
        return;
    }
    if (glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) != GLFW_PRESS)
    {
        frame_drag_.active = false;
        return;
    }

    int window_x = 0;
    int window_y = 0;
    glfwGetWindowPos(window_, &window_x, &window_y);
    double cursor_x = 0.0;
    double cursor_y = 0.0;
    glfwGetCursorPos(window_, &cursor_x, &cursor_y);
    const auto dx = static_cast<float>((window_x + cursor_x) - frame_drag_.origin_cursor_x);
    const auto dy = static_cast<float>((window_y + cursor_y) - frame_drag_.origin_cursor_y);
    const FrameEdge edge = frame_drag_.moving ? FrameEdge::None : frame_drag_.edge;
    const FrameRect origin{.x=static_cast<float>(frame_drag_.origin_x), .y=static_cast<float>(frame_drag_.origin_y),
                           .width=static_cast<float>(frame_drag_.origin_w), .height=static_cast<float>(frame_drag_.origin_h),};
    const FrameRect next = applyFrameDrag(edge, origin, dx, dy, static_cast<float>(min_width_),
                                          static_cast<float>(min_height_));

    const auto next_x = static_cast<int>(std::lround(next.x));
    const auto next_y = static_cast<int>(std::lround(next.y));
    const auto next_w = static_cast<int>(std::lround(next.width));
    const auto next_h = static_cast<int>(std::lround(next.height));
    if (next_x != window_x || next_y != window_y)
    {
        glfwSetWindowPos(window_, next_x, next_y);
    }

    int current_w = 0;
    int current_h = 0;
    glfwGetWindowSize(window_, &current_w, &current_h);
    if (next_w != current_w || next_h != current_h)
    {
        glfwSetWindowSize(window_, next_w, next_h);
    }
}

void Window::pollEvents()
{
    glfwPollEvents();
}

VkSurfaceKHR Window::createSurface(VkInstance instance, const VkAllocationCallbacks* allocator) const
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    checkVkResult(glfwCreateWindowSurface(instance, window_, allocator, &surface));
    return surface;
}

void Window::installBorderlessFrame()
{
#ifdef _WIN32
    HWND hwnd = glfwGetWin32Window(window_);
    if (hwnd == nullptr)
    {
        return;
    }

    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    style |= WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    style &= ~(WS_CAPTION | WS_DLGFRAME);
    SetWindowLongPtrW(hwnd, GWL_STYLE, style);

    // Square frame. Win11 rounds borderless windows unless told not to, and a
    // light-mode caption is the white bar along the top.
    const int corner = DWMWCP_DONOTROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
    const BOOL immersive_dark = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &immersive_dark, sizeof(immersive_dark));
    DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &kFrameColor, sizeof(kFrameColor));
    DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &kFrameColor, sizeof(kFrameColor));

    if (g_original_wnd_proc == nullptr)
    {
        // SetWindowLongPtrW returns the previous proc as LONG_PTR.
        g_original_wnd_proc = reinterpret_cast<WNDPROC>( // NOLINT(performance-no-int-to-ptr)
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(frameWndProc)));
    }

    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
#else
    (void)window_;
#endif
}

bool Window::beginNativeDrag(bool moving, FrameEdge edge) noexcept
{
#ifdef _WIN32
    HWND hwnd = glfwGetWin32Window(window_);
    if (hwnd == nullptr)
    {
        return false;
    }
    POINT cursor{};
    if (GetCursorPos(&cursor) == FALSE)
    {
        return false;
    }
    ReleaseCapture();
    SendMessageW(hwnd, WM_NCLBUTTONDOWN, hitTestCode(moving, edge), MAKELPARAM(cursor.x, cursor.y));
    return true;
#elif defined(__linux__)
    Display* display = glfwGetX11Display();
    const ::Window xwindow = glfwGetX11Window(window_);
    if (display == nullptr || xwindow == 0)
    {
        return false;
    }

    ::Window root = 0;
    ::Window child = 0;
    int root_x = 0;
    int root_y = 0;
    int window_x = 0;
    int window_y = 0;
    unsigned int mask = 0;
    if (XQueryPointer(display, xwindow, &root, &child, &root_x, &root_y, &window_x, &window_y, &mask) == 0)
    {
        return false;
    }

    static Atom moveresize = 0;
    if (moveresize == 0)
    {
        moveresize = XInternAtom(display, "_NET_WM_MOVERESIZE", 0);
    }
    if (moveresize == 0)
    {
        return false;
    }

    XUngrabPointer(display, CurrentTime);
    XFlush(display);

    XEvent event{};
    event.xclient.type = ClientMessage;
    event.xclient.window = xwindow;
    event.xclient.message_type = moveresize;
    event.xclient.format = 32;
    event.xclient.data.l[0] = root_x;
    event.xclient.data.l[1] = root_y;
    event.xclient.data.l[2] = x11MoveResizeDirection(moving, edge);
    event.xclient.data.l[3] = 1;
    event.xclient.data.l[4] = 1;

    const int sent = XSendEvent(display, DefaultRootWindow(display), 0,
                                SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(display);
    return sent != 0;
#else
    (void)moving;
    (void)edge;
    return false;
#endif
}

void Window::beginManualDrag(bool moving, FrameEdge edge) noexcept
{
    frame_drag_.active = true;
    frame_drag_.moving = moving;
    frame_drag_.edge = edge;
    glfwGetWindowPos(window_, &frame_drag_.origin_x, &frame_drag_.origin_y);
    glfwGetWindowSize(window_, &frame_drag_.origin_w, &frame_drag_.origin_h);
    double cursor_x = 0.0;
    double cursor_y = 0.0;
    glfwGetCursorPos(window_, &cursor_x, &cursor_y);
    frame_drag_.origin_cursor_x = frame_drag_.origin_x + cursor_x;
    frame_drag_.origin_cursor_y = frame_drag_.origin_y + cursor_y;
}

}  // namespace terminal
