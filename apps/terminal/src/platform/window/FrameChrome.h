// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>

namespace terminal {

// Outer band of a borderless window. Corners are their own rects so a corner
// hit is not also an edge. FrameEdge::None is the interior, and for a drag it
// means "move the window" rather than resize it.
enum class FrameEdge : unsigned char {
    None,
    Left,
    Right,
    Top,
    Bottom,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
};

struct FrameBorderRect {
    FrameEdge edge = FrameEdge::None;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

struct FrameBorders {
    std::array<FrameBorderRect, 8> rects{};
    int count = 0;
};

struct FrameRect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

// Screen rectangle in pixels. Right and bottom are exclusive, matching RECT.
struct FramePxRect {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
};

[[nodiscard]] inline FrameBorders frameBorderRects(float width, float height, float border) noexcept
{
    FrameBorders out;
    if (border <= 0.0f || width <= 0.0f || height <= 0.0f)
    {
        return out;
    }

    const float band_x = std::min(border, width * 0.5f);
    const float band_y = std::min(border, height * 0.5f);
    const float edge_w = width - band_x * 2.0f;
    const float edge_h = height - band_y * 2.0f;

    const auto push = [&out](FrameEdge edge, float x, float y, float w, float h) {
        if (w <= 0.0f || h <= 0.0f || out.count >= static_cast<int>(out.rects.size()))
        {
            return;
        }
        out.rects[static_cast<std::size_t>(out.count)] = FrameBorderRect{edge, x, y, w, h};
        ++out.count;
    };

    push(FrameEdge::TopLeft, 0.0f, 0.0f, band_x, band_y);
    push(FrameEdge::TopRight, width - band_x, 0.0f, band_x, band_y);
    push(FrameEdge::BottomLeft, 0.0f, height - band_y, band_x, band_y);
    push(FrameEdge::BottomRight, width - band_x, height - band_y, band_x, band_y);
    push(FrameEdge::Top, band_x, 0.0f, edge_w, band_y);
    push(FrameEdge::Bottom, band_x, height - band_y, edge_w, band_y);
    push(FrameEdge::Left, 0.0f, band_y, band_x, edge_h);
    push(FrameEdge::Right, width - band_x, band_y, band_x, edge_h);
    return out;
}

[[nodiscard]] inline FrameEdge hitTestFrameEdge(float x, float y, float width, float height,
                                                float border) noexcept
{
    if (x < 0.0f || y < 0.0f || x >= width || y >= height)
    {
        return FrameEdge::None;
    }

    const FrameBorders borders = frameBorderRects(width, height, border);
    for (int i = 0; i < borders.count; ++i)
    {
        const FrameBorderRect& rect = borders.rects[static_cast<std::size_t>(i)];
        const bool inside = x >= rect.x && y >= rect.y && x < rect.x + rect.width && y < rect.y + rect.height;
        if (inside)
        {
            return rect.edge;
        }
    }
    return FrameEdge::None;
}

// None moves the window. Any other edge resizes, keeping the opposite edges fixed
// and clamping to min_width / min_height.
[[nodiscard]] inline FrameRect applyFrameDrag(FrameEdge edge, const FrameRect& origin, float dx, float dy,
                                              float min_width, float min_height) noexcept
{
    FrameRect next = origin;
    if (edge == FrameEdge::None)
    {
        next.x += dx;
        next.y += dy;
        return next;
    }

    const bool left =
        edge == FrameEdge::Left || edge == FrameEdge::TopLeft || edge == FrameEdge::BottomLeft;
    const bool right =
        edge == FrameEdge::Right || edge == FrameEdge::TopRight || edge == FrameEdge::BottomRight;
    const bool top = edge == FrameEdge::Top || edge == FrameEdge::TopLeft || edge == FrameEdge::TopRight;
    const bool bottom =
        edge == FrameEdge::Bottom || edge == FrameEdge::BottomLeft || edge == FrameEdge::BottomRight;

    if (right)
    {
        next.width = std::max(min_width, origin.width + dx);
    }
    if (bottom)
    {
        next.height = std::max(min_height, origin.height + dy);
    }
    if (left)
    {
        next.width = std::max(min_width, origin.width - dx);
        next.x = origin.x + (origin.width - next.width);
    }
    if (top)
    {
        next.height = std::max(min_height, origin.height - dy);
        next.y = origin.y + (origin.height - next.height);
    }
    return next;
}

// Client rect of a maximized borderless window.
// `window` is the proposed window rect. `work` is the monitor work area.
// An undecorated maximize is already the work area. A thick frame hangs past
// it, off the monitor and over the taskbar. The client is the overlap.
// Insetting the work-area window by the resize border leaves a gap, and the
// top of that gap is the non-client strip painted white.
[[nodiscard]] inline FramePxRect maximizedClientRect(const FramePxRect& window,
                                                     const FramePxRect& work) noexcept
{
    const FramePxRect client{
        .left = std::max(window.left, work.left),
        .top = std::max(window.top, work.top),
        .right = std::min(window.right, work.right),
        .bottom = std::min(window.bottom, work.bottom),
    };
    if (client.right <= client.left || client.bottom <= client.top)
    {
        return window;
    }
    return client;
}

}  // namespace terminal
