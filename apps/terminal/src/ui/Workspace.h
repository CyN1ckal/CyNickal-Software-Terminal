// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "ui/ChartbookHost.h"
#include "ui/CommandPalette.h"
#include "ui/Commands.h"
#include "ui/InventoryPanel.h"

namespace terminal {

class Window;

class Workspace
{
public:
    WorkspaceClose draw(Window& window);
    [[nodiscard]] static const ImVec4& clearColor() noexcept;

private:
    void handleShellChords(ShellRequests& shell);
    void appendShellCommands(CommandList& out);
    void drawShellWindows(ShellRequests& shell);

    InventoryPanel inventory_;
    ChartbookHost books_;
    CommandPalette palette_;
    bool preferences_open_{false};
    bool shortcuts_open_{false};
};

}  // namespace terminal
