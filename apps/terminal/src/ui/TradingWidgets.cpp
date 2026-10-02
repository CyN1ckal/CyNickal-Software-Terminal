// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "ui/TradingWidgets.h"

#include "ui/Theme.h"
#include "ui/TradingFormat.h"

#include "imgui.h"

#include <string>

namespace terminal {

void drawRightText(const std::string& text, const ImVec4& color)
{
    ImFont* const mono = Theme::monoFont();
    if (mono != nullptr)
    {
        ImGui::PushFont(mono);
    }
    const float width = ImGui::GetContentRegionAvail().x;
    const float text_w = ImGui::CalcTextSize(text.c_str()).x;
    if (text_w < width)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + width - text_w);
    }
    ImGui::TextColored(color, "%s", text.c_str());
    if (mono != nullptr)
    {
        ImGui::PopFont();
    }
}

void drawMoneyCell(double amount, bool signed_color)
{
    ImVec4 color = Theme::text();
    if (amount < 0.0)
    {
        color = Theme::down();
    }
    else if (signed_color && amount > 0.0)
    {
        color = Theme::up();
    }
    drawRightText(formatMoney(amount), color);
}

bool primaryButton(const char* label)
{
    ImGui::PushStyleColor(ImGuiCol_Button, Theme::go());
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::accentHover());
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::accentPressed());
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::bg0());
    const bool pressed = ImGui::Button(label);
    ImGui::PopStyleColor(4);
    return pressed;
}

}  // namespace terminal
