// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "chart/CSymbolLink.h"
#include "ui/Theme.h"

#include "imgui.h"

namespace terminal {

[[nodiscard]] inline ImVec4 symbolLinkGroupColor(SymbolLinkGroup group) noexcept
{
    switch (group)
    {
    case SymbolLinkGroup::One:
        return Theme::accent();
    case SymbolLinkGroup::Two:
        return Theme::ok();
    case SymbolLinkGroup::Three:
        return Theme::warn();
    case SymbolLinkGroup::Four:
        return Theme::danger();
    case SymbolLinkGroup::None:
        return Theme::textDim();
    }
    return Theme::textDim();
}

inline void drawSymbolLinkSwatch(SymbolLinkGroup group)
{
    if (group == SymbolLinkGroup::None)
    {
        return;
    }
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const float side = ImGui::GetFrameHeight();
    const ImVec2 max(min.x + side, min.y + side);
    ImGui::GetWindowDrawList()->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(symbolLinkGroupColor(group)));
    ImGui::Dummy(ImVec2(side, side));
    ImGui::SetItemTooltip("Symbol link group %s", symbolLinkGroupLabel(group));
    ImGui::SameLine();
}

inline constexpr const char* kSymbolLinkTooltip =
    "Symbol link group. Panels in the same group change symbol together.";

// None, 1, 2, 3, 4. Choosing the current row calls setGroup with the same value, which does not publish.
// `combo_width` is the combo frame. Callers that share a tight row pass a smaller width.
inline void drawSymbolLinkCombo(CSymbolLink::Binding& binding, float combo_width = 72.0f)
{
    if (!binding.attached())
    {
        return;
    }
    ImGui::SameLine();
    const SymbolLinkGroup current = binding.group();
    drawSymbolLinkSwatch(current);
    ImGui::SetNextItemWidth(Theme::px(combo_width > 1.0f ? combo_width : 72.0f));
    if (!ImGui::BeginCombo("##symbol_link", symbolLinkGroupLabel(current)))
    {
        // While closed, the combo frame is the last item.
        ImGui::SetItemTooltip("%s", kSymbolLinkTooltip);
        return;
    }
    constexpr SymbolLinkGroup kGroups[] = {
        SymbolLinkGroup::None,
        SymbolLinkGroup::One,
        SymbolLinkGroup::Two,
        SymbolLinkGroup::Three,
        SymbolLinkGroup::Four,
    };
    for (const SymbolLinkGroup group : kGroups)
    {
        if (ImGui::Selectable(symbolLinkGroupLabel(group), group == current))
        {
            binding.setGroup(group);
        }
    }
    ImGui::EndCombo();
}

}  // namespace terminal
