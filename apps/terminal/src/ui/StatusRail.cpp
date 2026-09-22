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
            return {std::string(open_error), Theme::kDown};
        }
        return {"ingest worker is not running", Theme::kDown};
    }

    const IngestWorker::Snapshot snap = worker->snapshot();
    if (!snap.error.empty())
    {
        std::string text = snap.error;
        appendQueued(text, snap.queued);
        return {std::move(text), Theme::kDown};
    }

    const bool active = snap.running || (!snap.message.empty() && snap.message != "idle");
    if (active)
    {
        std::string text = snap.message.empty() ? "starting " + snap.symbol : snap.message;
        appendQueued(text, snap.queued);
        const ImVec4 color = snap.running ? Theme::kAccent : Theme::kText;
        return {std::move(text), color};
    }

    const std::string_view inventory_status = inventory.statusText();
    if (inventory_status.empty())
    {
        return {"idle", Theme::kMuted};
    }
    if (inventory_status == open_error)
    {
        return {std::string(inventory_status), Theme::kDown};
    }
    if (inventory_status == "idle" || inventory_status == "no coverage yet")
    {
        return {std::string(inventory_status), Theme::kMuted};
    }
    return {std::string(inventory_status), Theme::kText};
}

[[nodiscard]] ImVec4 chartTone(ChartLoadStatus status)
{
    switch (status)
    {
    case ChartLoadStatus::Error:
    case ChartLoadStatus::UnknownSymbol:
    case ChartLoadStatus::AmbiguousSymbol:
    case ChartLoadStatus::Unsupported:
        return Theme::kDown;
    case ChartLoadStatus::Ready:
        return Theme::kText;
    case ChartLoadStatus::Unconfigured:
    case ChartLoadStatus::Empty:
    case ChartLoadStatus::Busy:
        return Theme::kMuted;
    }
    return Theme::kMuted;
}

[[nodiscard]] RailField chartField(const CChartPane* pane)
{
    if (pane == nullptr)
    {
        return {"no chart", Theme::kMuted};
    }
    if (!pane->keyNote().empty())
    {
        return {std::string(pane->keyNote()), Theme::kDown};
    }

    const std::string_view line = pane->statusLine();
    if (isDownloadLine(line))
    {
        return {std::string(line), Theme::kAccent};
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

    if (pane->status() == ChartLoadStatus::Ready)
    {
        if (pane->barCount() > 0)
        {
            text += "  ";
            text += std::to_string(pane->barCount());
            text += " bars";
        }
        return {std::move(text), Theme::kText};
    }

    if (!line.empty())
    {
        text += "  ";
        text += line;
    }
    return {std::move(text), chartTone(pane->status())};
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
        if (gmtime_r(&wall, &utc) == nullptr)
        {
            return "--";
        }
        char buf[80];
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d  %02d:%02d:%02d UTC", utc.tm_year + 1900,
                      utc.tm_mon + 1, utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec);
        return buf;
    }
}

void drawField(const std::string& text, const ImVec4& color, float width, float y)
{
    const float x = ImGui::GetCursorPosX();
    ImGui::SetCursorPosY(y);
    const ImVec2 screen = ImGui::GetCursorScreenPos();
    const float text_h = ImGui::GetTextLineHeight();
    ImGui::PushClipRect(screen, ImVec2(screen.x + width, screen.y + text_h), true);
    const auto text_n = static_cast<int>(text.size());
    ImGui::TextColored(color, "%.*s", text_n, text.data());
    ImGui::PopClipRect();
    ImGui::SetCursorPos(ImVec2(x + width, y));
}

void drawGap()
{
    const ImVec2 window = ImGui::GetWindowPos();
    const float height = ImGui::GetWindowHeight();
    const ImVec2 screen = ImGui::GetCursorScreenPos();
    const float x = screen.x + kRailGap * 0.5f;
    ImGui::GetWindowDrawList()->AddLine(ImVec2(x, window.y + 3.0f), ImVec2(x, window.y + height - 3.0f),
                                        ImGui::GetColorU32(Theme::kLine));
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

    const float inner = std::max(0.0f, width - kRailPadX * 2.0f);
    const float remain = std::max(0.0f, inner - clock_w - kRailGap * 2.0f);
    const float chart_natural = ImGui::CalcTextSize(chart.text.c_str()).x;
    const float chart_w = std::min(chart_natural, remain * 0.46f);
    const float action_w = std::max(0.0f, remain - chart_w);

    const float text_h = ImGui::GetTextLineHeight();
    const float y = std::max(0.0f, (ImGui::GetWindowHeight() - text_h) * 0.5f);
    ImGui::SetCursorPos(ImVec2(kRailPadX, y));
    drawField(action.text, action.color, action_w, y);
    drawGap();
    drawField(chart.text, chart.color, chart_w, y);
    drawGap();
    if (mono != nullptr)
    {
        ImGui::PushFont(mono);
    }
    drawField(clock, Theme::kTextDim, clock_w, y);
    if (mono != nullptr)
    {
        ImGui::PopFont();
    }

    ImGui::End();
}

}  // namespace terminal
