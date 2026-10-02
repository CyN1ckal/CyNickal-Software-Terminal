// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "ui/AppearanceSettings.h"

#include <string>

namespace terminal::Appearance {

// The settings in effect. Read from data/terminal.json on first use.
[[nodiscard]] const AppearanceSettings& current();

// Takes effect at the start of the next frame. Saving waits until edits pause,
// so dragging a slider writes the file once.
void set(const AppearanceSettings& settings);

// Font zoom in whole pixels: Ctrl+= / Ctrl+- / Ctrl+0.
void stepFont(int delta_px);
void resetFont();

// Restyles ImGui when a change is pending and writes a due save. Call before
// ImGui::NewFrame. Returns true when the style was rebuilt.
bool applyPending(float dpi_scale);

// Writes a save that is still waiting. Call before exit.
void flush();

// Restyles now, for the first frame.
void applyNow(float dpi_scale);

// Empty when the last load and save succeeded.
[[nodiscard]] const std::string& lastError();

}  // namespace terminal::Appearance
