#pragma once

#include "ui/InventoryPanel.h"

#include "imgui.h"

namespace myapp {

class Workspace
{
public:
    void draw();
    [[nodiscard]] static const ImVec4& clearColor() noexcept;

private:
    InventoryPanel inventory_;
    bool dock_layout_applied_ = false;
};

}  // namespace myapp
