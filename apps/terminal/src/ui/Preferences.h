// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

namespace terminal {

// File > Preferences (Ctrl+,). Theme, gain/loss colors, density, and text size.
// Changes apply on the next frame and are saved to data/terminal.json.
void drawPreferences(bool& open);

// View > Keyboard Shortcuts (F1).
void drawShortcutsHelp(bool& open);

}  // namespace terminal
