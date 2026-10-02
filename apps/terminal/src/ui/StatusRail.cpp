// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "ui/StatusRail.h"

#include "chart/CChartBook.h"
#include "chart/CChartPane.h"
#include "data/IngestWorker.h"
#include "ui/Appearance.h"
#include "ui/InventoryPanel.h"
#include "ui/Theme.h"

#include "market_data/Time.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <exception>
#include <string>
#include <string_view>
#include <utility>

namespace terminal {
namespace {

constexpr float kRailPadX = 8.0f;
constexpr float kRailGap = 12.0f;

struct RailField
{
    std::string text;
    ImVec4 color{Theme::muted()};
    bool warn_gap{false};
};

void appendQueued(std::string& text, int queued)
{
    if (queued <= 0 || text.find("queued") != std::string::npos)
    {
        return;
    }
    text += " · queued ";
    text += std::to_string(queued);
}

[[nodiscard]] bool isDownloadLine(std::string_view text)
{
    return text.find("downloading") != std::string_view::npos;
}

[[nodiscard]] RailField actionField(const InventoryPanel& inventory)
{
    const IngestWorker* worker = inventory.ingestWorker();
    const std::string_view open_error = inventory.openError();
    if (worker == nullptr)
    {
        if (!open_error.empty())
        {
            return {.text=std::string(open_error), .color=Theme::danger()};
        }
        return {.text="ingest worker is not running", .color=Theme::danger()};
    }

    const IngestWorker::Snapshot snap = worker->snapshot();
    if (!snap.error.empty())
    {
        std::string text = snap.error;
        appendQueued(text, snap.queued);
        return {.text=std::move(text), .color=Theme::danger()};
    }

    const bool active = snap.running || (!snap.message.empty() && snap.message != "idle");
    if (active)
    {
        std::string text = snap.message.empty() ? "starting " + snap.symbol : snap.message;
        appendQueued(text, snap.queued);
        const ImVec4 color = snap.running ? Theme::accent() : Theme::text();
        return {.text=std::move(text), .color=color};
    }

    const std::string_view inventory_status = inventory.statusText();
    if (inventory_status == open_error && !inventory_status.empty())
    {
        return {.text=std::string(inventory_status), .color=Theme::danger()};
    }
    const bool resting = inventory_status.empty() || inventory_status == "idle" ||
                         inventory_status == "no coverage yet";
    if (resting && inventory.hasPartialCoverage())
    {
        return {.text="partial coverage", .color=Theme::text(), .warn_gap=true};
    }
    if (inventory_status.empty() || inventory_status == "idle")
    {
        return {};
    }
    if (inventory_status == "no coverage yet")
    {
        return {.text=std::string(inventory_status), .color=Theme::muted()};
    }
    return {.text=std::string(inventory_status), .color=Theme::text()};
}

[[nodiscard]] RailField chartField(const CChartPane* pane)
{
    if (pane == nullptr)
    {
        return {.text="no chart", .color=Theme::muted()};
    }
    if (!pane->keyNote().empty())
    {
        return {.text=std::string(pane->keyNote()), .color=Theme::danger()};
    }

    const std::string_view line = pane->statusLine();
    if (isDownloadLine(line))
    {
        return {.text=std::string(line), .color=Theme::accent()};
    }

    std::string text;
    const CChartSettings& settings = pane->settings();
    if (settings.symbol.empty())
    {
        text = "CHART ";
        text += std::to_string(pane->id());
    }
    else
    {
        text = settings.symbol;
        text += "  ";
        text += chartPeriodCode(settings.period);
    }

    constexpr std::string_view kCoaching = "Type a symbol and press Enter, or open Chart Settings.";
    const bool coaching = line == kCoaching;
    const ChartLoadStatus status = pane->status();
    const bool failed = status == ChartLoadStatus::Error || status == ChartLoadStatus::UnknownSymbol ||
                        status == ChartLoadStatus::Unsupported;
    if (!line.empty() && !coaching && status != ChartLoadStatus::Unconfigured)
    {
        // A loaded chart's line already opens with "SYM  1d"; do not say it twice.
        if (line.starts_with(text))
        {
            text = line;
        }
        else
        {
            text += "  ";
            text += line;
        }
    }
    return {.text=std::move(text), .color=failed ? Theme::danger() : Theme::muted()};
}

[[nodiscard]] std::string formatEtClock()
{
    const UnixSeconds ts = nowUtc();
    try
    {
        const std::chrono::time_zone* zone = std::chrono::locate_zone("America/New_York");
        const std::chrono::sys_seconds tp{std::chrono::seconds{ts}};
        const auto local = zone->to_local(tp);
        const auto day = std::chrono::floor<std::chrono::days>(local);
        const std::chrono::year_month_day ymd{day};
        const std::chrono::hh_mm_ss hms{local - day};
        const auto year = static_cast<int>(ymd.year());
        const auto month = static_cast<int>(static_cast<unsigned>(ymd.month()));
        const auto day_n = static_cast<int>(static_cast<unsigned>(ymd.day()));
        const auto hour = static_cast<int>(hms.hours().count());
        const auto minute = static_cast<int>(hms.minutes().count());
        const auto second = static_cast<int>(hms.seconds().count());
        char buf[80];
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d  %02d:%02d:%02d ET", year, month, day_n,
                      hour, minute, second);
        return buf;
    }
    catch (const std::exception&)
    {
        const auto wall = static_cast<std::time_t>(ts);
        std::tm utc{};
        if (!tryUtcTm(wall, utc))
        {
            return "--";
        }
        char buf[80];
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d  %02d:%02d:%02d UTC", utc.tm_year + 1900,
                      utc.tm_mon + 1, utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec);
        return buf;
    }
}

struct FrameStats
{
    std::string text;
    bool slow{false};
};

// Mean and worst frame over the last half second. Worst frames are what a user
// feels as a hitch, so they are shown next to the mean.
[[nodiscard]] FrameStats sampleFrameStats()
{
    constexpr float kWindowS = 0.5f;
    constexpr float kSlowMs = 33.4f;  // below 30 fps
    static float elapsed = 0.0f;
    static float worst = 0.0f;
    static int frames = 0;
    static FrameStats shown{.text = "-- ms"};
    const float dt = ImGui::GetIO().DeltaTime;
    elapsed += dt;
    worst = std::max(worst, dt);
    ++frames;
    if (elapsed >= kWindowS && frames > 0)
    {
        const float mean_ms = (elapsed / static_cast<float>(frames)) * 1000.0f;
        const float worst_ms = worst * 1000.0f;
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.1f ms  max %.1f", static_cast<double>(mean_ms),
                      static_cast<double>(worst_ms));
        shown = {.text = buf, .slow = worst_ms > kSlowMs};
        elapsed = 0.0f;
        worst = 0.0f;
        frames = 0;
    }
    return shown;
}

void drawField(const std::string& text, const ImVec4& color, float width, float y, bool tip_when_clipped)
{
    const float x = ImGui::GetCursorPosX();
    ImGui::SetCursorPosY(y);
    const ImVec2 screen = ImGui::GetCursorScreenPos();
    const float text_h = ImGui::GetTextLineHeight();
    ImGui::PushClipRect(screen, ImVec2(screen.x + width, screen.y + text_h), true);
    const auto text_n = static_cast<int>(text.size());
    ImGui::TextColored(color, "%.*s", text_n, text.data());
    ImGui::PopClipRect();
    const float natural = ImGui::CalcTextSize(text.c_str()).x;
    if (tip_when_clipped && natural > width + 0.5f && GImGui->HoveredWindow == nullptr &&
        ImGui::IsMouseHoveringRect(screen, ImVec2(screen.x + width, screen.y + text_h)))
    {
        ImGui::SetTooltip("%s", text.c_str());
    }
    ImGui::SetCursorPos(ImVec2(x + width, y));
}

void drawGap(const ImVec4& color)
{
    const ImVec2 window = ImGui::GetWindowPos();
    const float height = ImGui::GetWindowHeight();
    const ImVec2 screen = ImGui::GetCursorScreenPos();
    const float x = screen.x + (Theme::px(kRailGap) * 0.5f);
    ImGui::GetWindowDrawList()->AddLine(ImVec2(x, window.y + 3.0f), ImVec2(x, window.y + height - 3.0f),
                                        ImGui::GetColorU32(color));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + Theme::px(kRailGap));
}

}  // namespace

void drawStatusRail(const InventoryPanel& inventory, const CChartBook& charts)
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float height = ImGui::GetFrameHeight();
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Theme::px(kRailPadX), 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::bg1());
    // p_open is null inside BeginViewportSideBar, and NoSavedSettings drops any ini visibility.
    const bool open =
        ImGui::BeginViewportSideBar("##StatusRail", viewport, ImGuiDir_Down, height, flags);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    if (!open)
    {
        ImGui::End();
        return;
    }

    const ImVec2 origin = ImGui::GetWindowPos();
    const float width = ImGui::GetWindowWidth();
    ImGui::GetWindowDrawList()->AddLine(origin, ImVec2(origin.x + width, origin.y),
                                        ImGui::GetColorU32(Theme::line()));

    const RailField action = actionField(inventory);
    const RailField chart = chartField(charts.focusedPane());
    const std::string clock = formatEtClock();
    const bool show_stats = Appearance::current().frame_stats;
    const FrameStats stats = show_stats ? sampleFrameStats() : FrameStats{};

    ImFont* mono = Theme::monoFont();
    if (mono != nullptr)
    {
        ImGui::PushFont(mono);
    }
    const float clock_w = ImGui::CalcTextSize(clock.c_str()).x;
    const float stats_w = show_stats ? ImGui::CalcTextSize("000.0 ms  max 000.0").x : 0.0f;
    if (mono != nullptr)
    {
        ImGui::PopFont();
    }

    const float pad_x = Theme::px(kRailPadX);
    const float gap = Theme::px(kRailGap);
    const float inner = std::max(0.0f, width - (pad_x * 2.0f));
    const bool show_action = !action.text.empty();
    const float gaps = (show_action ? (gap * 2.0f) : gap) + (show_stats ? gap : 0.0f);
    const float remain = std::max(0.0f, inner - clock_w - stats_w - gaps);
    const float chart_natural = ImGui::CalcTextSize(chart.text.c_str()).x;
    const float chart_w = show_action ? std::min(chart_natural, remain * 0.46f) : std::min(chart_natural, remain);
    const float action_w = show_action ? std::max(0.0f, remain - chart_w) : 0.0f;

    const float text_h = ImGui::GetTextLineHeight();
    const float y = std::max(0.0f, (ImGui::GetWindowHeight() - text_h) * 0.5f);
    ImGui::SetCursorPos(ImVec2(pad_x, y));
    if (show_action)
    {
        drawField(action.text, action.color, action_w, y, false);
        drawGap(action.warn_gap ? Theme::warn() : Theme::line());
    }
    drawField(chart.text, chart.color, chart_w, y, true);
    // Clock and frame time stay pinned to the right edge.
    const float right_cluster = gap + (show_stats ? stats_w + gap : 0.0f) + clock_w;
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), width - pad_x - right_cluster));
    drawGap(Theme::line());
    if (mono != nullptr)
    {
        ImGui::PushFont(mono);
    }
    if (show_stats)
    {
        drawField(stats.text, stats.slow ? Theme::warn() : Theme::textDim(), stats_w, y, false);
        drawGap(Theme::line());
    }
    drawField(clock, Theme::textDim(), clock_w, y, false);
    if (mono != nullptr)
    {
        ImGui::PopFont();
    }

    ImGui::End();
}

}  // namespace terminal
