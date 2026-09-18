#include "Workspace.h"

#include "Theme.h"

#include "imgui_internal.h"

#include <cmath>

namespace myapp {
namespace {

constexpr const char* kMonitorTitle = "MONITOR";
constexpr const char* kChartTitle = "CHART";
constexpr const char* kDetailTitle = "DETAIL";
constexpr const char* kLogTitle = "LOG";

struct Quote
{
    const char* name;
    float last;
    float change;
    float pct;
};

constexpr Quote kQuotes[] = {
    {"ALPHA", 184.20f, 1.06f, 0.58f},
    {"BRAVO", 41.07f, -0.45f, -1.08f},
    {"CHARLIE", 96.14f, 0.22f, 0.23f},
    {"DELTA", 12.88f, -0.31f, -2.35f},
    {"ECHO", 250.40f, 3.12f, 1.26f},
    {"FOXTROT", 8.41f, -0.04f, -0.47f},
    {"GOLF", 63.75f, 0.00f, 0.00f},
    {"HOTEL", 119.03f, 1.88f, 1.61f},
    {"INDIA", 27.55f, -0.92f, -3.23f},
    {"JULIET", 54.18f, 0.14f, 0.26f},
};

void pushMonoFont()
{
    if (ImFont* mono = Theme::monoFont())
        ImGui::PushFont(mono);
}

void popMonoFont()
{
    if (Theme::monoFont() != nullptr)
        ImGui::PopFont();
}

void drawStrip(const char* code, const char* hint)
{
    ImGui::TextUnformatted(code);
    ImGui::SameLine();
    ImGui::TextColored(Theme::kMuted, "%s", hint);
    ImGui::Separator();
}

void textSigned(float value, const char* fmt)
{
    const ImVec4 color = value > 0.0f ? Theme::kUp : (value < 0.0f ? Theme::kDown : Theme::kInk);
    pushMonoFont();
    ImGui::TextColored(color, fmt, value);
    popMonoFont();
}

}  // namespace

const ImVec4& Workspace::clearColor() const noexcept
{
    return Theme::kCanvas;
}

void Workspace::draw()
{
    drawDockHost();

    if (show_monitor_)
        drawMonitor();
    if (show_chart_)
        drawChart();
    if (show_detail_)
        drawDetail();
    if (show_log_)
        drawLog();
}

void Workspace::drawDockHost()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                             ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Workspace", nullptr, flags);
    ImGui::PopStyleVar(3);

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Monitor", nullptr, &show_monitor_);
            ImGui::MenuItem("Chart", nullptr, &show_chart_);
            ImGui::MenuItem("Detail", nullptr, &show_detail_);
            ImGui::MenuItem("Log", nullptr, &show_log_);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset layout"))
                reset_layout_ = true;
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    const ImGuiID dockspace_id = ImGui::GetID("WorkspaceDock");
    if (reset_layout_)
    {
        ImGui::DockBuilderRemoveNode(dockspace_id);
        reset_layout_ = false;
        show_monitor_ = show_chart_ = show_detail_ = show_log_ = true;
    }
    if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr)
        buildDefaultLayout(dockspace_id, viewport->WorkPos, viewport->WorkSize);

    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    ImGui::End();
}

void Workspace::buildDefaultLayout(ImGuiID dockspace_id, ImVec2 pos, ImVec2 size)
{
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodePos(dockspace_id, pos);
    ImGui::DockBuilderSetNodeSize(dockspace_id, size);

    ImGuiID main = dockspace_id;
    const ImGuiID bottom = ImGui::DockBuilderSplitNode(main, ImGuiDir_Down, 0.22f, nullptr, &main);
    const ImGuiID left = ImGui::DockBuilderSplitNode(main, ImGuiDir_Left, 0.28f, nullptr, &main);
    const ImGuiID right = ImGui::DockBuilderSplitNode(main, ImGuiDir_Right, 0.32f, nullptr, &main);

    ImGui::DockBuilderDockWindow(kMonitorTitle, left);
    ImGui::DockBuilderDockWindow(kChartTitle, main);
    ImGui::DockBuilderDockWindow(kDetailTitle, right);
    ImGui::DockBuilderDockWindow(kLogTitle, bottom);
    ImGui::DockBuilderFinish(dockspace_id);
}

void Workspace::drawMonitor()
{
    if (!ImGui::Begin(kMonitorTitle, &show_monitor_))
    {
        ImGui::End();
        return;
    }

    drawStrip("WATCH", "LIVE");

    constexpr ImGuiTableFlags flags =
        ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX;
    if (ImGui::BeginTable("quotes", 4, flags, ImVec2(0.0f, ImGui::GetContentRegionAvail().y)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Last", ImGuiTableColumnFlags_WidthFixed, 72.0f);
        ImGui::TableSetupColumn("Chg", ImGuiTableColumnFlags_WidthFixed, 64.0f);
        ImGui::TableSetupColumn("%", ImGuiTableColumnFlags_WidthFixed, 64.0f);
        ImGui::TableHeadersRow();

        for (const Quote& quote : kQuotes)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(quote.name);

            ImGui::TableNextColumn();
            pushMonoFont();
            ImGui::TextColored(Theme::kInk, "%.2f", quote.last);
            popMonoFont();

            ImGui::TableNextColumn();
            textSigned(quote.change, "%+.2f");

            ImGui::TableNextColumn();
            if (quote.pct > 0.0f)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(Theme::kHeatUp));
            else if (quote.pct < 0.0f)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(Theme::kHeatDown));
            textSigned(quote.pct, "%+.2f%%");
        }
        ImGui::EndTable();
    }

    ImGui::End();
}

void Workspace::drawChart()
{
    if (!ImGui::Begin(kChartTitle, &show_chart_))
    {
        ImGui::End();
        return;
    }

    drawStrip("SERIES", "AMBER / CYAN");

    float values_a[96];
    float values_b[96];
    for (int i = 0; i < 96; ++i)
    {
        const float t = static_cast<float>(i);
        values_a[i] = 0.52f + 0.28f * std::sin(t * 0.13f) + 0.08f * std::sin(t * 0.47f);
        values_b[i] = 0.48f + 0.22f * std::cos(t * 0.11f);
    }

    ImGui::PushStyleColor(ImGuiCol_FrameBg, Theme::kCanvas);
    const ImVec2 plot_size(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y * 0.55f);
    ImGui::PlotLines("##amber", values_a, 96, 0, nullptr, 0.0f, 1.0f, plot_size);

    ImGui::PushStyleColor(ImGuiCol_PlotLines, Theme::kSeries);
    ImGui::PlotLines("##series", values_b, 96, 0, nullptr, 0.0f, 1.0f,
                     ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y));
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();

    ImGui::End();
}

void Workspace::drawDetail()
{
    if (!ImGui::Begin(kDetailTitle, &show_detail_))
    {
        ImGui::End();
        return;
    }

    drawStrip("DES", "ALPHA");

    constexpr ImGuiTableFlags flags =
        ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;
    if (ImGui::BeginTable("des", 2, flags))
    {
        ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        const auto row = [](const char* label, auto draw_value) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(label);
            ImGui::TableNextColumn();
            draw_value();
        };

        row("Name", [] { ImGui::TextColored(Theme::kInk, "ALPHA"); });
        row("Last", [] {
            pushMonoFont();
            ImGui::TextColored(Theme::kInk, "184.20");
            popMonoFont();
        });
        row("Change", [] { textSigned(1.06f, "%+.2f"); });
        row("Change %", [] { textSigned(0.58f, "%+.2f%%"); });
        row("Open", [] {
            pushMonoFont();
            ImGui::TextColored(Theme::kInk, "183.14");
            popMonoFont();
        });
        row("High", [] {
            pushMonoFont();
            ImGui::TextColored(Theme::kInk, "185.02");
            popMonoFont();
        });
        row("Low", [] {
            pushMonoFont();
            ImGui::TextColored(Theme::kInk, "182.40");
            popMonoFont();
        });
        row("Volume", [] {
            pushMonoFont();
            ImGui::TextColored(Theme::kInk, "12,441,200");
            popMonoFont();
        });
        row("Currency", [] { ImGui::TextColored(Theme::kMuted, "USD"); });
        row("Updated", [] { ImGui::TextColored(Theme::kMuted, "09:41:08"); });

        ImGui::EndTable();
    }

    ImGui::End();
}

void Workspace::drawLog()
{
    if (!ImGui::Begin(kLogTitle, &show_log_))
    {
        ImGui::End();
        return;
    }

    drawStrip("STAT", "SESSION");

    struct Line
    {
        const char* time;
        const char* text;
    };
    constexpr Line kLines[] = {
        {"09:40:51", "workspace ready"},
        {"09:40:52", "dock layout applied"},
        {"09:41:02", "monitor feed live"},
        {"09:41:08", "alpha +1.06"},
        {"09:41:08", "india -0.92"},
    };

    constexpr ImGuiTableFlags flags =
        ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingStretchProp;
    const float footer = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    if (ImGui::BeginTable("log", 2, flags, ImVec2(0.0f, ImGui::GetContentRegionAvail().y - footer)))
    {
        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 72.0f);
        ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
        for (const Line& line : kLines)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(Theme::kMuted, "%s", line.time);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(line.text);
        }
        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::TextColored(Theme::kMuted, "%.0f fps", ImGui::GetIO().Framerate);

    ImGui::End();
}

}  // namespace myapp
