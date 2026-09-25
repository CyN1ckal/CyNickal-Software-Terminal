// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "ui/StatusRail.h"

#include "chart/CChartBook.h"
#include "chart/CChartPane.h"
#include "data/IngestWorker.h"
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
    ImVec4 color{Theme::kMuted};
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
            return {.text=std::string(open_error), .color=Theme::kDown};
        }
        return {.text="ingest worker is not running", .color=Theme::kDown};
    }

    const IngestWorker::Snapshot snap = worker->snapshot();
    if (!snap.error.empty())
    {
        std::string text = snap.error;
        appendQueued(text, snap.queued);
        return {.text=std::move(text), .color=Theme::kDown};
    }

    const bool active = snap.running || (!snap.message.empty() && snap.message != "idle");
    if (active)
    {
        std::string text = snap.message.empty() ? "starting " + snap.symbol : snap.message;
        appendQueued(text, snap.queued);
        const ImVec4 color = snap.running ? Theme::kAccent : Theme::kText;
        return {.text=std::move(text), .color=color};
    }

    const std::string_view inventory_status = inventory.statusText();
    if (inventory_status == open_error && !inventory_status.empty())
    {
        return {.text=std::string(inventory_status), .color=Theme::kDown};
    }
    const bool resting = inventory_status.empty() || inventory_status == "idle" ||
                         inventory_status == "no coverage yet";
    if (resting && inventory.hasPartialCoverage())
    {
        return {.text="partial coverage", .color=Theme::kText, .warn_gap=true};
    }
    if (inventory_status.empty() || inventory_status == "idle")
    {
        return {};
    }
    if (inventory_status == "no coverage yet")
    {
        return {.text=std::string(inventory_status), .color=Theme::kMuted};
    }
    return {.text=std::string(inventory_status), .color=Theme::kText};
}

[[nodiscard]] RailField chartField(const CChartPane* pane)
{
    if (pane == nullptr)
    {
        return {.text="no chart", .color=Theme::kMuted};
    }
    if (!pane->keyNote().empty())
    {
        return {.text=std::string(pane->keyNote()), .color=Theme::kDown};
    }

    const std::string_view line = pane->statusLine();
    if (isDownloadLine(line))
    {
        return {.text=std::string(line), .color=Theme::kAccent};
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
        text += "  ";
        text += line;
    }
    return {.text=std::move(text), .color=failed ? Theme::kDown : Theme::kMuted};
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
    const float x = screen.x + (kRailGap * 0.5f);
    ImGui::GetWindowDrawList()->AddLine(ImVec2(x, window.y + 3.0f), ImVec2(x, window.y + height - 3.0f),
                                        ImGui::GetColorU32(color));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + kRailGap);
}

}  // namespace

void drawStatusRail(const InventoryPanel& inventory, const CChartBook& charts)
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float height = ImGui::GetFrameHeight();
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kRailPadX, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::kBg1);
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
                                        ImGui::GetColorU32(Theme::kLine));

    const RailField action = actionField(inventory);
    const RailField chart = chartField(charts.focusedPane());
    const std::string clock = formatEtClock();

    ImFont* mono = Theme::monoFont();
    if (mono != nullptr)
    {
        ImGui::PushFont(mono);
    }
    const float clock_w = ImGui::CalcTextSize(clock.c_str()).x;
    if (mono != nullptr)
    {
        ImGui::PopFont();
    }

    const float inner = std::max(0.0f, width - (kRailPadX * 2.0f));
    const bool show_action = !action.text.empty();
    const float gaps = show_action ? (kRailGap * 2.0f) : kRailGap;
    const float remain = std::max(0.0f, inner - clock_w - gaps);
    const float chart_natural = ImGui::CalcTextSize(chart.text.c_str()).x;
    const float chart_w = show_action ? std::min(chart_natural, remain * 0.46f) : std::min(chart_natural, remain);
    const float action_w = show_action ? std::max(0.0f, remain - chart_w) : 0.0f;

    const float text_h = ImGui::GetTextLineHeight();
    const float y = std::max(0.0f, (ImGui::GetWindowHeight() - text_h) * 0.5f);
    ImGui::SetCursorPos(ImVec2(kRailPadX, y));
    if (show_action)
    {
        drawField(action.text, action.color, action_w, y, false);
        drawGap(action.warn_gap ? Theme::kWarn : Theme::kLine);
    }
    drawField(chart.text, chart.color, chart_w, y, true);
    drawGap(Theme::kLine);
    if (mono != nullptr)
    {
        ImGui::PushFont(mono);
    }
    drawField(clock, Theme::kTextDim, clock_w, y, false);
    if (mono != nullptr)
    {
        ImGui::PopFont();
    }

    ImGui::End();
}

}  // namespace terminal
