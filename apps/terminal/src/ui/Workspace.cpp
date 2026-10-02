// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "ui/Workspace.h"

#include "platform/window/Window.h"
#include "ui/Appearance.h"
#include "ui/Preferences.h"
#include "ui/StatusRail.h"
#include "ui/Theme.h"
#include "ui/TitleBar.h"

#include <string>

namespace terminal {
namespace {

[[nodiscard]] bool anyPopupOpen()
{
    return ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
}

[[nodiscard]] bool chord(ImGuiKeyChord key)
{
    return ImGui::Shortcut(key, ImGuiInputFlags_RouteGlobal);
}

}  // namespace

const ImVec4& Workspace::clearColor() noexcept
{
    return Theme::canvas();
}

void Workspace::handleShellChords(ShellRequests& shell)
{
    // Text size works everywhere, including inside dialogs, so it can be fixed when it is unreadable.
    if (chord(ImGuiMod_Ctrl | ImGuiKey_Equal) || chord(ImGuiMod_Ctrl | ImGuiKey_KeypadAdd))
    {
        Appearance::stepFont(1);
    }
    if (chord(ImGuiMod_Ctrl | ImGuiKey_Minus) || chord(ImGuiMod_Ctrl | ImGuiKey_KeypadSubtract))
    {
        Appearance::stepFont(-1);
    }
    if (chord(ImGuiMod_Ctrl | ImGuiKey_0) || chord(ImGuiMod_Ctrl | ImGuiKey_Keypad0))
    {
        Appearance::resetFont();
    }
    if (anyPopupOpen())
    {
        return;
    }
    if (chord(ImGuiMod_Ctrl | ImGuiKey_K) || chord(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_P))
    {
        shell.palette = true;
    }
    if (chord(ImGuiMod_Ctrl | ImGuiKey_Comma))
    {
        preferences_open_ = !preferences_open_;
    }
    if (chord(ImGuiKey_F1))
    {
        shortcuts_open_ = !shortcuts_open_;
    }
}

void Workspace::appendShellCommands(CommandList& out)
{
    const AppearanceSettings& now = Appearance::current();
    const auto add = [&out](const char* group, std::string label, const char* shortcut, bool enabled,
                            std::function<void()> run) {
        out.push_back(Command{.group = group,
                              .label = std::move(label),
                              .shortcut = shortcut != nullptr ? shortcut : "",
                              .enabled = enabled,
                              .run = std::move(run),});
    };
    add("Help", "Keyboard Shortcuts", kShortcutHelp, true, [this] { shortcuts_open_ = true; });
    add("Settings", "Preferences...", kShortcutPreferences, true, [this] { preferences_open_ = true; });
    add("Settings", "Larger Text", kShortcutTextLarger, now.font_px < kFontPxMax, [] { Appearance::stepFont(1); });
    add("Settings", "Smaller Text", kShortcutTextSmaller, now.font_px > kFontPxMin, [] { Appearance::stepFont(-1); });
    add("Settings", "Default Text Size", kShortcutTextReset, now.font_px != kFontPxDefault,
        [] { Appearance::resetFont(); });
    for (int index = 0; index < static_cast<int>(ThemeChoice::Count); ++index)
    {
        const auto theme = static_cast<ThemeChoice>(index);
        add("Settings", std::string("Theme: ") + themeLabel(theme), nullptr, theme != now.theme, [theme] {
            AppearanceSettings next = Appearance::current();
            next.theme = theme;
            Appearance::set(next);
        });
    }
    for (int index = 0; index < static_cast<int>(MarketColors::Count); ++index)
    {
        const auto market = static_cast<MarketColors>(index);
        add("Settings", std::string("Colors: ") + marketColorsLabel(market), nullptr, market != now.market, [market] {
            AppearanceSettings next = Appearance::current();
            next.market = market;
            Appearance::set(next);
        });
    }
    for (int index = 0; index < static_cast<int>(Density::Count); ++index)
    {
        const auto density = static_cast<Density>(index);
        add("Settings", std::string("Density: ") + densityLabel(density), nullptr, density != now.density, [density] {
            AppearanceSettings next = Appearance::current();
            next.density = density;
            Appearance::set(next);
        });
    }
    add("Settings", now.frame_stats ? "Hide Frame Time" : "Show Frame Time", nullptr, true, [] {
        AppearanceSettings next = Appearance::current();
        next.frame_stats = !next.frame_stats;
        Appearance::set(next);
    });
}

void Workspace::drawShellWindows(ShellRequests& shell)
{
    if (shell.preferences)
    {
        preferences_open_ = true;
        ImGui::SetWindowFocus("###preferences");
    }
    if (shell.shortcuts)
    {
        shortcuts_open_ = true;
        ImGui::SetWindowFocus("###shortcuts");
    }
    drawPreferences(preferences_open_);
    drawShortcutsHelp(shortcuts_open_);

    if (shell.palette)
    {
        palette_.open();
    }
    if (!palette_.wantsCommands())
    {
        return;
    }
    CommandList commands;
    commands.reserve(64);
    books_.appendCommands(commands, inventory_);
    appendShellCommands(commands);
    palette_.draw(commands);
}

WorkspaceClose Workspace::draw(Window& window)
{
    ShellRequests shell;
    // One caption bar for the window buttons and the menus. It stays up across every chartbook.
    drawTitleBar(window, books_, inventory_, shell);
    const WorkspaceClose close = books_.drawChrome(inventory_, window.shouldClose());
    drawStatusRail(inventory_, books_.activeBook());
    books_.handleShortcuts(inventory_);
    handleShellChords(shell);
    books_.drawSpace(inventory_);
    drawShellWindows(shell);
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
