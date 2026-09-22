// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

namespace terminal {

class ChartbookHost;
class InventoryPanel;
class Window;

// Caption, window buttons, and the File / Chart / View menus. One bar.
void drawTitleBar(Window& window, ChartbookHost& books, InventoryPanel& inventory);

// BeginMenu / EndMenu for a menu drawn on the title bar. The bar uses a taller
// frame padding; these restore the normal padding for the dropdown items.
[[nodiscard]] bool beginTitleMenu(const char* label);
void endTitleMenu();

// Resize bands around the main viewport. Submit after docked panels.
void drawWindowResizeBorders(Window& window);

}  // namespace terminal
