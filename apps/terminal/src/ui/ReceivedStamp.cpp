// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "ui/ReceivedStamp.h"

#include "ui/Theme.h"

#include "imgui.h"

namespace terminal {

void drawReceivedStamp(std::optional<UnixSeconds> received)
{
    if (!received.has_value())
    {
        return;
    }
    const std::string text = formatReceivedUtc(*received);
    if (text.empty())
    {
        return;
    }
    const float stamp_w = ImGui::CalcTextSize(text.c_str()).x;
    const float avail = ImGui::GetContentRegionAvail().x;
    if (avail > stamp_w)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - stamp_w);
    }
    ImGui::TextColored(Theme::kTextDim, "%s", text.c_str());
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Data received");
    }
}

}  // namespace terminal
