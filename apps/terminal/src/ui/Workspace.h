#pragma once

#include "chart/CChartBook.h"
#include "ui/InventoryPanel.h"

#include "imgui.h"

namespace terminal {

class Workspace
{
public:
    void draw();
    [[nodiscard]] static const ImVec4& clearColor() noexcept;

private:
    InventoryPanel inventory_;
    CChartBook charts_;
    bool dock_layout_applied_ = false;
    ImGuiID chart_dock_id_ = 0;
};

}  // namespace terminal
