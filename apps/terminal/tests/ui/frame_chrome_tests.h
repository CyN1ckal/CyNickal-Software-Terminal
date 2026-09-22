// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "platform/window/FrameChrome.h"

TEST_CASE("frame border bands meet at corners and leave the interior clear")
{
    using terminal::FrameEdge;
    using terminal::hitTestFrameEdge;

    constexpr float kWidth = 200.0f;
    constexpr float kHeight = 100.0f;
    constexpr float kBorder = 8.0f;

    CHECK(hitTestFrameEdge(4.0f, 4.0f, kWidth, kHeight, kBorder) == FrameEdge::TopLeft);
    CHECK(hitTestFrameEdge(100.0f, 4.0f, kWidth, kHeight, kBorder) == FrameEdge::Top);
    CHECK(hitTestFrameEdge(196.0f, 4.0f, kWidth, kHeight, kBorder) == FrameEdge::TopRight);
    CHECK(hitTestFrameEdge(4.0f, 50.0f, kWidth, kHeight, kBorder) == FrameEdge::Left);
    CHECK(hitTestFrameEdge(196.0f, 50.0f, kWidth, kHeight, kBorder) == FrameEdge::Right);
    CHECK(hitTestFrameEdge(4.0f, 96.0f, kWidth, kHeight, kBorder) == FrameEdge::BottomLeft);
    CHECK(hitTestFrameEdge(100.0f, 96.0f, kWidth, kHeight, kBorder) == FrameEdge::Bottom);
    CHECK(hitTestFrameEdge(196.0f, 96.0f, kWidth, kHeight, kBorder) == FrameEdge::BottomRight);

    CHECK(hitTestFrameEdge(8.0f, 50.0f, kWidth, kHeight, kBorder) == FrameEdge::None);
    CHECK(hitTestFrameEdge(100.0f, 50.0f, kWidth, kHeight, kBorder) == FrameEdge::None);
    CHECK(hitTestFrameEdge(-1.0f, 4.0f, kWidth, kHeight, kBorder) == FrameEdge::None);
    CHECK(hitTestFrameEdge(200.0f, 4.0f, kWidth, kHeight, kBorder) == FrameEdge::None);
    CHECK(hitTestFrameEdge(100.0f, 4.0f, kWidth, kHeight, 0.0f) == FrameEdge::None);
}

TEST_CASE("applyFrameDrag moves or resizes while keeping the opposite edge fixed")
{
    using terminal::FrameEdge;
    using terminal::FrameRect;
    using terminal::applyFrameDrag;

    const FrameRect origin{100.0f, 80.0f, 400.0f, 300.0f};

    const FrameRect moved = applyFrameDrag(FrameEdge::None, origin, 15.0f, -3.0f, 100.0f, 100.0f);
    CHECK(moved.x == 115.0f);
    CHECK(moved.y == 77.0f);
    CHECK(moved.width == 400.0f);
    CHECK(moved.height == 300.0f);

    const FrameRect right = applyFrameDrag(FrameEdge::Right, origin, 25.0f, 0.0f, 100.0f, 100.0f);
    CHECK(right.x == 100.0f);
    CHECK(right.width == 425.0f);

    const FrameRect left = applyFrameDrag(FrameEdge::Left, origin, 10.0f, 0.0f, 100.0f, 100.0f);
    CHECK(left.width == 390.0f);
    CHECK(left.x == 110.0f);
    CHECK(left.x + left.width == origin.x + origin.width);

    const FrameRect clamped = applyFrameDrag(FrameEdge::Left, origin, 350.0f, 0.0f, 100.0f, 100.0f);
    CHECK(clamped.width == 100.0f);
    CHECK(clamped.x + clamped.width == origin.x + origin.width);

    const FrameRect corner = applyFrameDrag(FrameEdge::TopLeft, origin, 10.0f, -5.0f, 100.0f, 100.0f);
    CHECK(corner.width == 390.0f);
    CHECK(corner.height == 305.0f);
    CHECK(corner.x + corner.width == origin.x + origin.width);
    CHECK(corner.y + corner.height == origin.y + origin.height);
}

TEST_CASE("maximized client fills the work area and does not inset a frame gap")
{
    using terminal::FramePxRect;
    using terminal::maximizedClientRect;

    const FramePxRect work{0, 0, 1920, 1040};

    // Undecorated maximize is already the work area. Insetting it would open a gap.
    const FramePxRect filled = maximizedClientRect(work, work);
    CHECK(filled.left == work.left);
    CHECK(filled.top == work.top);
    CHECK(filled.right == work.right);
    CHECK(filled.bottom == work.bottom);

    // Thick-frame maximize hangs the resize border off the monitor and over the taskbar.
    constexpr int kFrame = 8;
    const FramePxRect hung{-kFrame, -kFrame, 1920 + kFrame, 1080 + kFrame};
    const FramePxRect hung_client = maximizedClientRect(hung, work);
    CHECK(hung_client.left == work.left);
    CHECK(hung_client.top == work.top);
    CHECK(hung_client.right == work.right);
    CHECK(hung_client.bottom == work.bottom);

    // Full monitor, no overhang: still stop at the taskbar.
    const FramePxRect monitor{0, 0, 1920, 1080};
    const FramePxRect clipped = maximizedClientRect(monitor, work);
    CHECK(clipped.bottom == work.bottom);
    CHECK(clipped.top == work.top);

    // Secondary monitor whose origin is not (0, 0), taskbar on the top edge.
    const FramePxRect work2{1920, 40, 3840, 1080};
    const FramePxRect hung2{1920 - kFrame, 40 - kFrame, 3840 + kFrame, 1080 + kFrame};
    const FramePxRect client2 = maximizedClientRect(hung2, work2);
    CHECK(client2.left == work2.left);
    CHECK(client2.top == work2.top);
    CHECK(client2.right == work2.right);
    CHECK(client2.bottom == work2.bottom);

    // No overlap: leave the proposed window alone.
    const FramePxRect elsewhere{10, 10, 20, 20};
    const FramePxRect other{100, 100, 200, 200};
    const FramePxRect kept = maximizedClientRect(elsewhere, other);
    CHECK(kept.left == elsewhere.left);
    CHECK(kept.top == elsewhere.top);
    CHECK(kept.right == elsewhere.right);
    CHECK(kept.bottom == elsewhere.bottom);
}
