// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "ui/PayoffPanel.h"

#include "ui/Theme.h"

#include <algorithm>
#include <cstdio>
#include <string>

namespace terminal {
namespace {

constexpr float kDividerHeight = 6.0f;
constexpr float kMinGraphHeight = 120.0f;
constexpr float kMinEntryHeight = 140.0f;

}  // namespace

PayoffPanel::PayoffPanel(int id) : id_(id) {}

int PayoffPanel::id() const noexcept
{
    return id_;
}

bool PayoffPanel::windowOpen() const noexcept
{
    return window_open_;
}

void PayoffPanel::closeWindow()
{
    window_open_ = false;
}

void PayoffPanel::requestFocus()
{
    focus_on_appear_ = true;
}

void PayoffPanel::importState(const ChartbookPayoff& state)
{
    source_.restore(state.symbol, state.figi, state.expiration, state.expiration_type);
    wizard_.restore(state.legs, state.spot);
}

ChartbookPayoff PayoffPanel::exportState() const
{
    ChartbookPayoff state;
    state.id = id_;
    state.symbol = source_.symbol();
    state.figi = source_.figi();
    state.expiration = source_.hasExpiration() ? source_.expiration() : 0;
    state.expiration_type = source_.hasExpiration() ? std::string(toSql(source_.expirationType())) : std::string{};
    state.spot = wizard_.manualSpot();
    state.legs = wizard_.legs();
    return state;
}

void PayoffPanel::setWindowScope(int runtime_id) noexcept
{
    runtime_id_ = runtime_id;
}

void PayoffPanel::setPlacement(bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size)
{
    place_force_ = force;
    place_floating_ = floating;
    place_dock_ = dock;
    place_pos_ = pos;
    place_size_ = size;
}

void PayoffPanel::drawEntry(IngestWorker* ingest)
{
    if (source_.drawPicker(ingest))
    {
        wizard_.clear();
    }
    ImGui::SameLine();
    ImVec4 status_color = Theme::kMuted;
    if (source_.fetching())
    {
        status_color = Theme::kAccent;
    }
    else if (source_.failed())
    {
        status_color = Theme::kDown;
    }
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(status_color, "%s", source_.status().c_str());
    ImGui::Separator();
    wizard_.drawEntry(source_.quotes(), source_.hasExpiration() ? source_.expiration() : 0);
}

bool PayoffPanel::draw(Store* store, std::string_view store_error, IngestWorker* ingest)
{
    if (focus_on_appear_)
    {
        ImGui::SetNextWindowFocus();
        focus_on_appear_ = false;
    }
    if (place_force_)
    {
        if (place_floating_)
        {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + place_pos_.x, viewport->WorkPos.y + place_pos_.y),
                                    ImGuiCond_Always);
            ImGui::SetNextWindowSize(place_size_, ImGuiCond_Always);
            ImGui::SetNextWindowDockID(0, ImGuiCond_Always);
            ImGui::SetNextWindowViewport(viewport->ID);
        }
        else if (place_dock_ != 0)
        {
            ImGui::SetNextWindowDockID(place_dock_, ImGuiCond_Always);
        }
        place_force_ = false;
    }

    char title[160];
    if (source_.symbol().empty())
    {
        std::snprintf(title, sizeof(title), "PAYOFF %d###cb%d_payoff%d", id_, runtime_id_, id_);
    }
    else
    {
        std::snprintf(title, sizeof(title), "%s PAYOFF###cb%d_payoff%d", source_.symbol().c_str(), runtime_id_,
                      id_);
    }
    if (!ImGui::Begin(title, &window_open_, ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::End();
        return false;
    }
    if (store == nullptr && !store_error.empty())
    {
        ImGui::TextColored(Theme::kDown, "%s", std::string(store_error).c_str());
    }

    // The picker in the entry section queues its changes; they land on this refresh next frame.
    source_.refresh(store, ingest);
    wizard_.syncChain(source_.quotes());

    const float avail = ImGui::GetContentRegionAvail().y;
    const float spacing = ImGui::GetStyle().ItemSpacing.y;
    const float usable = std::max(avail - kDividerHeight - (spacing * 2.0f), 1.0f);
    const float graph_max = std::max(kMinGraphHeight, usable - kMinEntryHeight);
    const float graph_h = std::clamp(usable * graph_share_, std::min(kMinGraphHeight, graph_max), graph_max);

    if (ImGui::BeginChild("payoff_graph", ImVec2(0.0f, graph_h), ImGuiChildFlags_Borders))
    {
        wizard_.drawGraph();
    }
    ImGui::EndChild();

    ImGui::InvisibleButton("##payoff_divider", ImVec2(-1.0f, kDividerHeight));
    const bool dragging = ImGui::IsItemActive();
    if (dragging || ImGui::IsItemHovered())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }
    if (dragging && usable > 1.0f)
    {
        graph_share_ = std::clamp((graph_h + ImGui::GetIO().MouseDelta.y) / usable, 0.15f, 0.85f);
    }
    const ImVec2 lo = ImGui::GetItemRectMin();
    const ImVec2 hi = ImGui::GetItemRectMax();
    const float mid = (lo.y + hi.y) * 0.5f;
    ImGui::GetWindowDrawList()->AddLine(ImVec2(lo.x, mid), ImVec2(hi.x, mid),
                                        ImGui::GetColorU32(dragging ? Theme::kAccent : Theme::kLine2));

    if (ImGui::BeginChild("payoff_entry", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders))
    {
        drawEntry(ingest);
    }
    ImGui::EndChild();

    const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
    ImGui::End();
    return focused;
}

}  // namespace terminal
