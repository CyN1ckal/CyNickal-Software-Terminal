// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include <functional>
#include <string>
#include <vector>

namespace terminal {

// One action the menus, the keyboard, and the command palette share. Built each
// frame, so `enabled` and the label reflect the current focus and selection.
struct Command
{
    std::string group;     // menu it lives under: File, View, Chart, Appearance
    std::string label;
    std::string shortcut;  // display text only; the owner handles the chord
    bool enabled{true};
    std::function<void()> run;
};

using CommandList = std::vector<Command>;

// Windows the workspace shell owns, opened from a menu or a chord.
struct ShellRequests
{
    bool palette{false};
    bool preferences{false};
    bool shortcuts{false};
};

// Menu labels for chords. The ImGuiKey chords live next to each handler.
inline constexpr const char* kShortcutPalette = "Ctrl+K";
inline constexpr const char* kShortcutPreferences = "Ctrl+,";
inline constexpr const char* kShortcutHelp = "F1";
inline constexpr const char* kShortcutNewBook = "Ctrl+N";
inline constexpr const char* kShortcutOpenBook = "Ctrl+O";
inline constexpr const char* kShortcutSave = "Ctrl+S";
inline constexpr const char* kShortcutSaveAs = "Ctrl+Shift+S";
inline constexpr const char* kShortcutRefresh = "Ctrl+R";
inline constexpr const char* kShortcutTextLarger = "Ctrl+=";
inline constexpr const char* kShortcutTextSmaller = "Ctrl+-";
inline constexpr const char* kShortcutTextReset = "Ctrl+0";

}  // namespace terminal
