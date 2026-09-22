// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "ui/Workspace.h"

#include "platform/window/Window.h"
#include "ui/StatusRail.h"
#include "ui/Theme.h"
#include "ui/TitleBar.h"

namespace terminal {

const ImVec4& Workspace::clearColor() noexcept
{
    return Theme::kCanvas;
}

WorkspaceClose Workspace::draw(Window& window)
{
    // One caption bar for the window buttons and the menus. It stays up across every chartbook.
    drawTitleBar(window, books_, inventory_);
    const WorkspaceClose close = books_.drawChrome(inventory_, window.shouldClose());
    drawStatusRail(inventory_, books_.activeBook());
    books_.drawSpace(inventory_);
    drawWindowResizeBorders(window);

    if (close == WorkspaceClose::Quit)
    {
        window.setShouldClose(true);
    }
    else if (close == WorkspaceClose::Cancel)
    {
        window.setShouldClose(false);
    }
    return close;
}

}  // namespace terminal
