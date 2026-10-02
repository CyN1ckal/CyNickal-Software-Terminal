// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "ui/CommandPalette.h"

#include "ui/Theme.h"

#include "imgui.h"

#include <algorithm>
#include <functional>
#include <string_view>

namespace terminal {
namespace {

constexpr const char* kPopupId = "##command_palette";
constexpr int kVisibleRows = 14;
constexpr std::size_t kRecentMax = 6;
constexpr int kRecentBonus = 1000;
// Equal to a whole-word match at the start of the text, so only whole-word group hits survive.
constexpr int kGroupPenalty = 300;

[[nodiscard]] std::string commandKey(const Command& command)
{
    return command.group + "/" + command.label;
}

[[nodiscard]] std::string searchText(const Command& command)
{
    return command.group + " " + command.label;
}

}  // namespace

void CommandPalette::open()
{
    open_request_ = true;
}

void CommandPalette::remember(const std::string& key)
{
    std::erase(recent_, key);
    recent_.insert(recent_.begin(), key);
    if (recent_.size() > kRecentMax)
    {
        recent_.resize(kRecentMax);
    }
}

void CommandPalette::rank(const CommandList& commands)
{
    const std::string_view query(query_);
    if (!rerank_ && query == ranked_query_ && commands.size() == ranked_count_)
    {
        return;
    }
    rerank_ = false;
    ranked_query_ = query;
    ranked_count_ = commands.size();
    matches_.clear();
    for (int index = 0; std::cmp_less(index, commands.size()); ++index)
    {
        const Command& command = commands[static_cast<std::size_t>(index)];
        // The label decides. The group name only helps as a whole word ("view fin"), or
        // "fin" would also match "File New chartbook" letter by letter.
        int score = commandMatchScore(query, command.label);
        if (score < 0)
        {
            const int grouped = commandMatchScore(query, searchText(command));
            score = grouped >= kGroupPenalty ? grouped - kGroupPenalty : -1;
        }
        if (score < 0)
        {
            continue;
        }
        // Recently run commands float up, most recent first.
        const auto recent = std::ranges::find(recent_, commandKey(command));
        if (recent != recent_.end())
        {
            score += kRecentBonus - static_cast<int>(recent - recent_.begin());
        }
        matches_.push_back({.index = index, .score = score});
    }
    std::ranges::stable_sort(matches_, std::greater<>{}, &Match::score);
    selected_ = 0;
    // Start on the first command that can run.
    for (int row = 0; std::cmp_less(row, matches_.size()); ++row)
    {
        if (commands[static_cast<std::size_t>(matches_[static_cast<std::size_t>(row)].index)].enabled)
        {
            selected_ = row;
            break;
        }
    }
    scroll_to_selected_ = true;
}

void CommandPalette::draw(const CommandList& commands)
{
    if (open_request_)
    {
        open_request_ = false;
        query_[0] = '\0';
        rerank_ = true;
        focus_input_ = true;
        ImGui::OpenPopup(kPopupId);
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float width = std::min(Theme::px(620.0f), viewport->WorkSize.x - Theme::px(32.0f));
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + (viewport->WorkSize.x * 0.5f),
                                   viewport->WorkPos.y + Theme::px(48.0f)),
                            ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(width, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Theme::px(8.0f), Theme::px(8.0f)));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, Theme::bg2());
    ImGui::PushStyleColor(ImGuiCol_Border, Theme::line2());
    const bool shown = ImGui::BeginPopup(kPopupId, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
    visible_ = shown;
    if (!shown)
    {
        return;
    }

    const Command* chosen = nullptr;

    // Query field.
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (focus_input_)
    {
        ImGui::SetKeyboardFocusHere();
        focus_input_ = false;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Theme::px(8.0f), Theme::px(6.0f)));
    ImGui::InputTextWithHint("##palette_query", "Type a command", query_, sizeof(query_));
    ImGui::PopStyleVar();
    rank(commands);

    const int count = static_cast<int>(matches_.size());
    const auto step = [&](int delta) {
        if (count == 0)
        {
            return;
        }
        selected_ = (selected_ + delta + count) % count;
        scroll_to_selected_ = true;
    };
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
    {
        step(1);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
    {
        step(-1);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_PageDown, true))
    {
        step(std::min(kVisibleRows, std::max(1, count - 1 - selected_)));
    }
    if (ImGui::IsKeyPressed(ImGuiKey_PageUp, true))
    {
        step(-std::min(kVisibleRows, std::max(1, selected_)));
    }
    const bool enter = ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);
    if (enter && selected_ >= 0 && selected_ < count)
    {
        const Command& command = commands[static_cast<std::size_t>(matches_[static_cast<std::size_t>(selected_)].index)];
        if (command.enabled)
        {
            chosen = &command;
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        ImGui::CloseCurrentPopup();
    }

    ImGui::Spacing();
    const float row_h = ImGui::GetTextLineHeight() + Theme::px(8.0f);
    const int rows = std::clamp(count, 1, kVisibleRows);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
    const bool list_open = ImGui::BeginChild("##palette_results", ImVec2(0.0f, row_h * static_cast<float>(rows)),
                                             ImGuiChildFlags_None, ImGuiWindowFlags_NoNav);
    ImGui::PopStyleVar();
    if (list_open)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
        if (count == 0)
        {
            ImGui::TextColored(Theme::muted(), "No command matches \"%s\".", query_);
        }
        ImGuiListClipper clipper;
        clipper.Begin(count, row_h);
        if (scroll_to_selected_)
        {
            clipper.IncludeItemByIndex(selected_);
        }
        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
            {
                const Command& command =
                    commands[static_cast<std::size_t>(matches_[static_cast<std::size_t>(row)].index)];
                ImGui::PushID(row);
                const ImVec2 start = ImGui::GetCursorScreenPos();
                const float full_w = ImGui::GetContentRegionAvail().x;
                const bool is_selected = row == selected_;
                ImGui::BeginDisabled(!command.enabled);
                if (ImGui::Selectable("##row", is_selected, ImGuiSelectableFlags_None, ImVec2(full_w, row_h)))
                {
                    chosen = &command;
                }
                ImGui::EndDisabled();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && ImGui::GetIO().MouseDelta.x != 0.0f)
                {
                    selected_ = row;
                }
                if (is_selected && scroll_to_selected_)
                {
                    ImGui::SetScrollHereY();
                    scroll_to_selected_ = false;
                }

                // Group, label, and the chord on the right.
                ImDrawList* draw = ImGui::GetWindowDrawList();
                const float text_y = start.y + ((row_h - ImGui::GetTextLineHeight()) * 0.5f);
                const float pad = Theme::px(8.0f);
                const ImU32 group_color = ImGui::GetColorU32(command.enabled ? Theme::textDim() : Theme::textFaint());
                const ImU32 label_color = ImGui::GetColorU32(command.enabled ? Theme::text() : Theme::textFaint());
                const float group_w = Theme::px(84.0f);
                draw->AddText(ImVec2(start.x + pad, text_y), group_color, command.group.c_str());
                draw->AddText(ImVec2(start.x + pad + group_w, text_y), label_color, command.label.c_str());
                if (!command.shortcut.empty())
                {
                    const float key_w = ImGui::CalcTextSize(command.shortcut.c_str()).x;
                    draw->AddText(ImVec2(start.x + full_w - key_w - pad, text_y), group_color,
                                  command.shortcut.c_str());
                }
                ImGui::PopID();
            }
        }
        ImGui::PopStyleVar();
    }
    ImGui::EndChild();

    ImGui::PushStyleColor(ImGuiCol_Text, Theme::textFaint());
    ImGui::TextUnformatted("Up/Down select   Enter run   Esc close");
    ImGui::PopStyleColor();

    if (chosen != nullptr)
    {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();

    if (chosen != nullptr && chosen->run)
    {
        remember(commandKey(*chosen));
        chosen->run();
    }
}

}  // namespace terminal
