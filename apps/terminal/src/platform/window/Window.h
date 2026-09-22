// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "platform/window/FrameChrome.h"

#include <utility>
#include <vector>

namespace terminal {

// OS window title and the custom caption. Task switchers read the GLFW title.
inline constexpr const char* kWindowTitle = "CyNickal Software Terminal";

class GlfwContext {
public:
    GlfwContext();
    ~GlfwContext();

    GlfwContext(const GlfwContext&) = delete;
    GlfwContext& operator=(const GlfwContext&) = delete;
    GlfwContext(GlfwContext&&) = delete;
    GlfwContext& operator=(GlfwContext&&) = delete;

    [[nodiscard]] static std::vector<const char*> requiredVulkanInstanceExtensions();
};

class Window {
public:
    Window(int width, int height, const char* title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    [[nodiscard]] GLFWwindow* handle() const noexcept { return window_; }
    [[nodiscard]] bool shouldClose() const noexcept;
    void setShouldClose(bool close) noexcept;
    [[nodiscard]] bool isIconified() const noexcept;
    [[nodiscard]] bool isMaximized() const noexcept;
    [[nodiscard]] std::pair<int, int> framebufferSize() const;

    void requestClose() noexcept;
    void minimize() noexcept;
    void toggleMaximize() noexcept;

    // Hand the pointer to the window manager. If that is unavailable, the drag
    // continues in pumpFrameDrag until the mouse button is released.
    void beginMove() noexcept;
    void beginResize(FrameEdge edge) noexcept;
    void pumpFrameDrag() noexcept;

    static void pollEvents();
    [[nodiscard]] VkSurfaceKHR createSurface(VkInstance instance,
                                             const VkAllocationCallbacks* allocator) const;

private:
    struct FrameDrag {
        bool active = false;
        bool moving = false;
        FrameEdge edge = FrameEdge::None;
        int origin_x = 0;
        int origin_y = 0;
        int origin_w = 0;
        int origin_h = 0;
        double origin_cursor_x = 0.0;
        double origin_cursor_y = 0.0;
    };

    void installBorderlessFrame();
    [[nodiscard]] bool beginNativeDrag(bool moving, FrameEdge edge) noexcept;
    void beginManualDrag(bool moving, FrameEdge edge) noexcept;

    GLFWwindow* window_ = nullptr;
    int min_width_ = 720;
    int min_height_ = 480;
    FrameDrag frame_drag_{};
};

}  // namespace terminal
