// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "ui/ChartbookHost.h"
#include "ui/InventoryPanel.h"

namespace terminal {

class Window;

class Workspace
{
public:
    WorkspaceClose draw(Window& window);
    [[nodiscard]] static const ImVec4& clearColor() noexcept;

private:
    InventoryPanel inventory_;
    ChartbookHost books_;
};

}  // namespace terminal
