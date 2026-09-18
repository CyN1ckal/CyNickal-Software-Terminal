#pragma once

#include "imgui.h"

namespace myapp {

class Workspace {
public:
    void draw();
    [[nodiscard]] static const ImVec4& clearColor() noexcept;

private:
    void drawDockHost();
    static void buildDefaultLayout(ImGuiID dockspace_id, ImVec2 pos, ImVec2 size);
    void drawMonitor();
    void drawChart();
    void drawDetail();
    void drawLog();

    bool reset_layout_ = false;
    bool show_monitor_ = true;
    bool show_chart_ = true;
    bool show_detail_ = true;
    bool show_log_ = true;
};

}  // namespace myapp
