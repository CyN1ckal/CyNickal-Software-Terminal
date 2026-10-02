// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "ui/Preferences.h"

#include "ui/Appearance.h"
#include "ui/Commands.h"
#include "ui/Theme.h"

#include "imgui.h"

#include <array>

namespace terminal {
namespace {

void sectionLabel(const char* text)
{
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::textDim());
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}

void beginWindow(const char* title, bool& open, float width)
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + (viewport->WorkSize.x * 0.5f),
                                   viewport->WorkPos.y + (viewport->WorkSize.y * 0.5f)),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(Theme::px(width), 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Theme::px(12.0f), Theme::px(10.0f)));
    constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings |
                                        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;
    ImGui::Begin(title, &open, kFlags);
    ImGui::PopStyleVar();
}

// Buttons that act as one choice. The selected one carries the accent.
template <typename Enum, std::size_t N>
[[nodiscard]] bool segmented(const char* id, const std::array<Enum, N>& values, Enum& value,
                             const char* (*label)(Enum))
{
    bool changed = false;
    ImGui::PushID(id);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(Theme::px(1.0f), 0.0f));
    const float width = (ImGui::GetContentRegionAvail().x - (Theme::px(1.0f) * static_cast<float>(N - 1))) /
                        static_cast<float>(N);
    for (std::size_t index = 0; index < N; ++index)
    {
        if (index > 0)
        {
            ImGui::SameLine();
        }
        const bool selected = values[index] == value;
        if (selected)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, Theme::accentWash());
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::accentWash());
            ImGui::PushStyleColor(ImGuiCol_Text, Theme::accent());
        }
        ImGui::PushID(static_cast<int>(index));
        if (ImGui::Button(label(values[index]), ImVec2(width, 0.0f)) && !selected)
        {
            value = values[index];
            changed = true;
        }
        ImGui::PopID();
        if (selected)
        {
            ImGui::PopStyleColor(3);
        }
    }
    ImGui::PopStyleVar();
    ImGui::PopID();
    return changed;
}

// "▲ 1.25  ▼ 0.84" in the scheme's colors, so the choice is seen, not described.
void marketSample(const Theme::Palette& palette)
{
    ImGui::TextColored(palette.up, "\xE2\x96\xB2 +1.25%%");
    ImGui::SameLine(0.0f, Theme::px(10.0f));
    ImGui::TextColored(palette.down, "\xE2\x96\xBC \xE2\x88\x92" "0.84%%");
}

void drawRow(const char* keys, const char* action)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImFont* const mono = Theme::monoFont();
    if (mono != nullptr)
    {
        ImGui::PushFont(mono);
    }
    ImGui::TextColored(Theme::accent(), "%s", keys);
    if (mono != nullptr)
    {
        ImGui::PopFont();
    }
    ImGui::TableSetColumnIndex(1);
    ImGui::TextUnformatted(action);
}

void drawGroup(const char* title)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Dummy(ImVec2(0.0f, Theme::px(4.0f)));
    sectionLabel(title);
}

}  // namespace

void drawPreferences(bool& open)
{
    if (!open)
    {
        return;
    }
    beginWindow("Preferences###preferences", open, 440.0f);
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsAnyItemActive() &&
        ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        open = false;
    }

    AppearanceSettings next = Appearance::current();
    constexpr ImGuiTableFlags kLayout = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings;
    if (ImGui::BeginTable("##prefs", 2, kLayout))
    {
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, Theme::px(108.0f));
        ImGui::TableSetupColumn("control", ImGuiTableColumnFlags_WidthStretch);
        const auto row = [](const char* label) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            sectionLabel(label);
            ImGui::TableSetColumnIndex(1);
        };

        row("Theme");
        constexpr std::array kThemes{ThemeChoice::StratumDark, ThemeChoice::HighContrast, ThemeChoice::Light};
        (void)segmented("theme", kThemes, next.theme, &themeLabel);

        row("Gains / losses");
        // The theme's check mark is drawn on the accent fill of a checkbox; a radio dot sits on the field.
        ImGui::PushStyleColor(ImGuiCol_CheckMark, Theme::accent());
        for (int index = 0; index < static_cast<int>(MarketColors::Count); ++index)
        {
            const auto market = static_cast<MarketColors>(index);
            ImGui::PushID(index);
            if (ImGui::RadioButton(marketColorsLabel(market), next.market == market))
            {
                next.market = market;
            }
            ImGui::Indent(ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x);
            marketSample(Theme::paletteFor(next.theme, market));
            ImGui::Unindent(ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::PopID();
        }
        ImGui::PopStyleColor();

        row("Density");
        constexpr std::array kDensities{Density::Compact, Density::Standard, Density::Comfortable};
        (void)segmented("density", kDensities, next.density, &densityLabel);

        row("Text size");
        const float button = ImGui::GetFrameHeight();
        if (ImGui::Button("-", ImVec2(button, 0.0f)))
        {
            next.font_px = clampFontPx(next.font_px - 1);
        }
        ImGui::SetItemTooltip("Smaller text (%s)", kShortcutTextSmaller);
        ImGui::SameLine();
        const char* reset_label = "Reset";
        const float reset_w = ImGui::CalcTextSize(reset_label).x + (ImGui::GetStyle().FramePadding.x * 2.0f);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - button - reset_w -
                                (ImGui::GetStyle().ItemSpacing.x * 2.0f));
        // Only the text size waits for release, so the layout does not jump while dragging.
        int font_px = next.font_px;
        ImGui::SliderInt("##font_px", &font_px, kFontPxMin, kFontPxMax, "%d px", ImGuiSliderFlags_AlwaysClamp);
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            next.font_px = font_px;
        }
        ImGui::SameLine();
        if (ImGui::Button("+", ImVec2(button, 0.0f)))
        {
            next.font_px = clampFontPx(next.font_px + 1);
        }
        ImGui::SetItemTooltip("Larger text (%s)", kShortcutTextLarger);
        ImGui::SameLine();
        ImGui::BeginDisabled(next.font_px == kFontPxDefault);
        if (ImGui::Button(reset_label))
        {
            next.font_px = kFontPxDefault;
        }
        ImGui::EndDisabled();

        row("Diagnostics");
        ImGui::Checkbox("Frame time on the status rail", &next.frame_stats);

        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Button("Restore Defaults"))
    {
        next = AppearanceSettings{};
    }
    ImGui::SameLine();
    const std::string& error = Appearance::lastError();
    if (!error.empty())
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(Theme::danger(), "%s", error.c_str());
    }
    else
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(Theme::textFaint(), "Saved to data/terminal.json");
    }

    Appearance::set(next);
    ImGui::End();
}

void drawShortcutsHelp(bool& open)
{
    if (!open)
    {
        return;
    }
    beginWindow("Keyboard Shortcuts###shortcuts", open, 460.0f);
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        open = false;
    }
    constexpr ImGuiTableFlags kLayout = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings |
                                        ImGuiTableFlags_RowBg;
    if (ImGui::BeginTable("##keys", 2, kLayout))
    {
        ImGui::TableSetupColumn("keys", ImGuiTableColumnFlags_WidthFixed, Theme::px(150.0f));
        ImGui::TableSetupColumn("action", ImGuiTableColumnFlags_WidthStretch);

        drawGroup("ANYWHERE");
        drawRow(kShortcutPalette, "Command palette: every action by name");
        drawRow(kShortcutPreferences, "Preferences");
        drawRow(kShortcutHelp, "This list");
        drawRow("Ctrl+Tab", "Cycle windows");
        drawRow(kShortcutRefresh, "Refetch the focused panel");
        drawRow("Ctrl+= / Ctrl+-", "Larger / smaller text");
        drawRow(kShortcutTextReset, "Default text size");

        drawGroup("CHARTBOOKS");
        drawRow(kShortcutNewBook, "New chartbook");
        drawRow(kShortcutOpenBook, "Open chartbook");
        drawRow(kShortcutSave, "Save");
        drawRow(kShortcutSaveAs, "Save as");

        drawGroup("FOCUSED CHART");
        drawRow("QQQ Enter", "Change symbol (downloads if not stored)");
        drawRow("5m Enter", "Change period: 1m 5m 15m 1h 1d");
        drawRow("Up / Down", "Zoom in / out");
        drawRow("Left / Right", "Scroll");
        drawRow("Home / End", "First / latest bar");
        drawRow("Wheel / Drag", "Zoom / pan");
        drawRow("Shift+Wheel", "Zoom, keeping the bar under the pointer");
        drawRow("F5", "Chart settings");
        drawRow("F6", "Studies");
        drawRow("Esc", "Clear typed keys");

        drawGroup("TABLES AND FORMS");
        drawRow("Tab / Shift+Tab", "Next / previous field");
        drawRow("Arrows, Space", "Move and activate with the keyboard");
        drawRow("Enter", "Submit a symbol field (GO)");

        ImGui::EndTable();
    }
    ImGui::End();
}

}  // namespace terminal
