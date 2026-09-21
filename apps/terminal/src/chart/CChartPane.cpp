// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/CChartPane.h"

#include "chart/CChartPlot.h"
#include "chart/CChartTransform.h"
#include "chart/CStudyCompute.h"
#include "chart/CStudySettings.h"
#include "ui/Theme.h"

#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <string>

namespace terminal {
namespace {

constexpr auto kReloadInterval = std::chrono::seconds(2);

[[nodiscard]] const char* periodDisplayName(ChartBarPeriod period) noexcept
{
    switch (period)
    {
    case ChartBarPeriod::Minute1:
        return "1 Minute";
    case ChartBarPeriod::Minute5:
        return "5 Minute";
    case ChartBarPeriod::Minute15:
        return "15 Minute";
    case ChartBarPeriod::Hour1:
        return "1 Hour";
    case ChartBarPeriod::Day1:
        return "Daily";
    }
    return "1 Minute";
}

[[nodiscard]] ImVec4 statusColor(ChartLoadStatus status)
{
    switch (status)
    {
    case ChartLoadStatus::Error:
    case ChartLoadStatus::UnknownSymbol:
    case ChartLoadStatus::AmbiguousSymbol:
    case ChartLoadStatus::Unsupported:
        return Theme::kDown;
    case ChartLoadStatus::Ready:
    case ChartLoadStatus::Unconfigured:
    case ChartLoadStatus::Empty:
    case ChartLoadStatus::Busy:
        return Theme::kMuted;
    }
    return Theme::kMuted;
}

}  // namespace

CChartPane::CChartPane(int id) : id_(id) {}

int CChartPane::id() const noexcept
{
    return id_;
}

bool CChartPane::windowOpen() const noexcept
{
    return window_open_;
}

const CChartSettings& CChartPane::settings() const noexcept
{
    return settings_;
}

ChartLoadStatus CChartPane::status() const noexcept
{
    return loaded_.status;
}

const std::vector<CStudyInstance>& CChartPane::studies() const noexcept
{
    return studies_;
}

bool CChartPane::settingsOpen() const noexcept
{
    return settings_open_;
}

bool CChartPane::studiesOpen() const noexcept
{
    return studies_open_;
}

void CChartPane::openSettings()
{
    if (studies_open_)
    {
        return;
    }
    draft_ = settings_;
    std::snprintf(draft_symbol_, sizeof(draft_symbol_), "%s", draft_.symbol.c_str());
    settings_open_ = true;
}

void CChartPane::openStudies()
{
    // Flag only. OpenPopup from the menu bar is a different ID stack than the pane.
    if (settings_open_)
    {
        return;
    }
    study_draft_ = studies_;
    if (study_draft_.empty())
    {
        study_draft_selected_ = -1;
    }
    else
    {
        study_draft_selected_ = 0;
    }
    studies_open_ = true;
}

void CChartPane::closeWindow()
{
    window_open_ = false;
}

void CChartPane::requestFocus()
{
    focus_on_appear_ = true;
}

void CChartPane::cancelDraft()
{
    settings_open_ = false;
}

void CChartPane::cancelStudyDraft()
{
    study_draft_.clear();
    study_draft_selected_ = -1;
    studies_open_ = false;
}

void CChartPane::applyStudyDraft()
{
    for (CStudyInstance& inst : study_draft_)
    {
        if (auto* params = std::get_if<MovingAverageParams>(&inst.params))
        {
            clampMovingAverageParams(*params);
        }
    }
    studies_ = study_draft_;
    computed_ = studiesForLoad(loaded_, studies_);
}

void CChartPane::reload(Store* store, std::string_view store_error)
{
    last_reload_ = std::chrono::steady_clock::now();
    if (store == nullptr)
    {
        loaded_ = ChartLoadResult{};
        loaded_.status = ChartLoadStatus::Error;
        loaded_.message = store_error.empty() ? "chart store failed to open" : std::string(store_error);
        loaded_settings_ = settings_;
        computed_ = studiesForLoad(loaded_, studies_);
        return;
    }

    const ChartLoadResult incoming = loadChartBars(*store, settings_);
    const bool same = settingsIdentityEqual(loaded_settings_, settings_) && !loaded_.bars.empty();
    if ((incoming.status == ChartLoadStatus::Busy || incoming.status == ChartLoadStatus::Error) &&
        same)
    {
        if (incoming.status == ChartLoadStatus::Error)
        {
            loaded_.status = ChartLoadStatus::Error;
            loaded_.message = incoming.message;
        }
        return;
    }
    const bool reset_view = loaded_settings_.symbol != settings_.symbol ||
                            loaded_settings_.period != settings_.period ||
                            loaded_settings_.session_count != settings_.session_count;
    loaded_ = incoming;
    loaded_settings_ = settings_;
    if (reset_view)
    {
        view_.scroll_from_end = 0;
        resetChartScale(view_);
    }
    computed_ = studiesForLoad(loaded_, studies_);
}

void CChartPane::applyDraft(Store* store, std::string_view store_error)
{
    draft_.symbol = normalizeChartSymbol(draft_symbol_);
    std::snprintf(draft_symbol_, sizeof(draft_symbol_), "%s", draft_.symbol.c_str());
    if (!isChartSettingsSupported(draft_))
    {
        return;
    }
    clampV1Limits(draft_);
    if (draft_.scale_range != settings_.scale_range)
    {
        resetChartScale(view_);
    }
    settings_ = draft_;
    reload(store, store_error);
}

void CChartPane::drawSettingsPopup(Store* store, std::string_view store_error)
{
    char popup_id[64];
    std::snprintf(popup_id, sizeof(popup_id), "Chart Settings###chart_settings_%d", id_);
    const bool want_modal = settings_open_;
    if (want_modal)
    {
        ImGui::OpenPopup(popup_id);
    }
    if (ImGui::BeginPopupModal(popup_id, &settings_open_))
    {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, Theme::kField);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Symbol");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        const bool enter = ImGui::InputText(
            "##chart_symbol", draft_symbol_, sizeof(draft_symbol_),
            ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopStyleColor();

        ImGui::TextUnformatted("Bar Period");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140.0f);
        if (ImGui::BeginCombo("##period", periodDisplayName(draft_.period)))
        {
            for (const ChartBarPeriod period :
                 {ChartBarPeriod::Minute1, ChartBarPeriod::Minute5, ChartBarPeriod::Minute15,
                  ChartBarPeriod::Hour1, ChartBarPeriod::Day1})
            {
                if (ImGui::Selectable(periodDisplayName(period), draft_.period == period))
                {
                    draft_.period = period;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::TextUnformatted("Bar Type");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140.0f);
        if (ImGui::BeginCombo("##bar_type", "Candlestick"))
        {
            bool selected = true;
            ImGui::Selectable("Candlestick", &selected);
            ImGui::EndCombo();
        }
        ImGui::TextColored(Theme::kMuted, "v1: candlesticks only");

        ImGui::TextUnformatted("Days to Load");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        ImGui::InputInt("##days", &draft_.session_count);
        ImGui::TextColored(Theme::kMuted, "Bar count and date range are reserved.");

        ImGui::Separator();
        ImGui::TextUnformatted("Bar Spacing (px)");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        ImGui::InputFloat("##spacing", &draft_.bar_spacing_px, 1.0f, 4.0f, "%.0f");

        ImGui::TextUnformatted("Candlestick Width %");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        float width_pct = draft_.bar_width_frac * 100.0f;
        if (ImGui::InputFloat("##width", &width_pct, 5.0f, 10.0f, "%.0f"))
        {
            draft_.bar_width_frac = width_pct / 100.0f;
        }

        const char* scale_label = "Automatic";
        if (draft_.scale_range == ChartScaleRange::ConstantRange)
        {
            scale_label = "Constant Range";
        }
        else if (draft_.scale_range == ChartScaleRange::UserDefined)
        {
            scale_label = "User Defined";
        }
        ImGui::TextUnformatted("Scale Range");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::BeginCombo("##scale_range", scale_label))
        {
            if (ImGui::Selectable("Automatic", draft_.scale_range == ChartScaleRange::Automatic))
            {
                draft_.scale_range = ChartScaleRange::Automatic;
            }
            if (ImGui::Selectable("Constant Range", draft_.scale_range == ChartScaleRange::ConstantRange))
            {
                draft_.scale_range = ChartScaleRange::ConstantRange;
            }
            if (ImGui::Selectable("User Defined", draft_.scale_range == ChartScaleRange::UserDefined))
            {
                draft_.scale_range = ChartScaleRange::UserDefined;
            }
            ImGui::EndCombo();
        }
        ImGui::TextColored(Theme::kMuted, "Sierra-style. Right-click the price scale to change interactively.");

        if (draft_.scale_range == ChartScaleRange::ConstantRange)
        {
            ImGui::TextUnformatted("Range");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            ImGui::InputDouble("##const_range", &draft_.constant_range, 0.0, 0.0, "%.4f");
        }
        if (draft_.scale_range == ChartScaleRange::UserDefined)
        {
            ImGui::TextUnformatted("Top");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            ImGui::InputDouble("##user_top", &draft_.user_top, 0.0, 0.0, "%.4f");
            ImGui::TextUnformatted("Bottom");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            ImGui::InputDouble("##user_bottom", &draft_.user_bottom, 0.0, 0.0, "%.4f");
        }

        ImGui::TextUnformatted("Scale Padding %");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        ImGui::InputFloat("##pad", &draft_.scale_padding_pct, 1.0f, 4.0f, "%.1f");

        if (!isChartSettingsSupported(draft_))
        {
            ImGui::TextColored(Theme::kDown, "candlestick bars and Days to Load only.");
        }

        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Button, Theme::kGo);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::kAccentHover);
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::kBg0);
        const bool ok = ImGui::Button("OK");
        ImGui::PopStyleColor(3);
        ImGui::SameLine();
        const bool apply = ImGui::Button("Apply");
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::kCancel);
        const bool cancel = ImGui::Button("Cancel");
        ImGui::PopStyleColor();

        if (enter || apply || ok)
        {
            applyDraft(store, store_error);
        }
        if (ok)
        {
            settings_open_ = false;
            ImGui::CloseCurrentPopup();
        }
        if (cancel)
        {
            cancelDraft();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    else if (want_modal && !settings_open_)
    {
        cancelDraft();
    }
}

void CChartPane::drawStudiesPopup()
{
    char popup_id[64];
    std::snprintf(popup_id, sizeof(popup_id), "Studies###chart_studies_%d", id_);
    const bool want_modal = studies_open_;
    if (want_modal)
    {
        ImGui::OpenPopup(popup_id);
    }
    if (ImGui::BeginPopupModal(popup_id, &studies_open_, ImGuiWindowFlags_AlwaysAutoResize))
    {
        const bool length_enter =
            drawStudyDraftBody(study_draft_, study_draft_selected_, next_study_id_);

        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Button, Theme::kGo);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::kAccentHover);
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::kBg0);
        const bool ok = ImGui::Button("OK");
        ImGui::PopStyleColor(3);
        ImGui::SameLine();
        const bool apply = ImGui::Button("Apply");
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::kCancel);
        const bool cancel = ImGui::Button("Cancel");
        ImGui::PopStyleColor();

        if (length_enter || apply || ok)
        {
            applyStudyDraft();
        }
        if (ok)
        {
            studies_open_ = false;
            ImGui::CloseCurrentPopup();
        }
        // RouteFocused still scores this modal while a combo or color popup is focused,
        // so registering Escape here would own the key and the child would not close.
        // NewFrame may already have closed that child on this press; skip one frame so
        // the same Esc does not also discard the draft.
        const bool child_popup = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
        ImGuiStorage* state = ImGui::GetStateStorage();
        const ImGuiID child_popup_id = ImGui::GetID("##studies_child_popup");
        const bool child_popup_was_open = state->GetBool(child_popup_id, false);
        state->SetBool(child_popup_id, child_popup);
        bool escape = false;
        if (!child_popup && !child_popup_was_open && ImGui::IsWindowFocused())
        {
            escape = ImGui::Shortcut(ImGuiKey_Escape);
        }
        if (cancel || escape)
        {
            cancelStudyDraft();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    else if (want_modal && !studies_open_)
    {
        cancelStudyDraft();
    }
}

void CChartPane::drawStatusLine() const
{
    const ImVec4 color = statusColor(loaded_.status);
    const char* text = loaded_.message.c_str();
    if (loaded_.message.empty())
    {
        switch (loaded_.status)
        {
        case ChartLoadStatus::Unconfigured:
            text = "Open Chart Settings to choose a symbol.";
            break;
        case ChartLoadStatus::Busy:
            text = "store busy";
            break;
        case ChartLoadStatus::Ready:
            text = "";
            break;
        case ChartLoadStatus::Empty:
        case ChartLoadStatus::UnknownSymbol:
        case ChartLoadStatus::AmbiguousSymbol:
        case ChartLoadStatus::Unsupported:
        case ChartLoadStatus::Error:
            text = loaded_.message.c_str();
            break;
        }
    }
    ImGui::TextColored(color, "%s", text);
}

void CChartPane::handleChartKeys()
{
    if (settings_open_ || studies_open_ || ImGui::GetIO().WantTextInput)
    {
        return;
    }
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
    {
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
    {
        settings_.bar_spacing_px += 1.0f;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
    {
        settings_.bar_spacing_px -= 1.0f;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
    {
        view_.scroll_from_end += 1;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
    {
        view_.scroll_from_end -= 1;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_End))
    {
        view_.scroll_from_end = 0;
        if (settings_.scale_range == ChartScaleRange::ConstantRange)
        {
            view_.move_offset = 0.0;
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Home) && !loaded_.bars.empty())
    {
        const ChartVisibleWindow win =
            computeVisibleWindow(static_cast<int>(loaded_.bars.size()), view_.last_plot_w,
                                 settings_.bar_spacing_px, 0, kChartRightFillBars);
        view_.scroll_from_end =
            std::max(0, static_cast<int>(loaded_.bars.size()) + kChartRightFillBars - win.slot_count);
    }
    clampV1Limits(settings_);
}

void CChartPane::drawPlotBody()
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::kPanel);
    if (ImGui::BeginChild("plot", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        if (loaded_.status == ChartLoadStatus::Ready && !loaded_.bars.empty())
        {
            std::string_view tz{"America/New_York"};
            if (loaded_.instrument.has_value())
            {
                const std::string& zone = loaded_.instrument.value().timezone;
                if (!zone.empty())
                {
                    tz = zone;
                }
            }
            drawCandlesticks(loaded_.bars, settings_, view_, tz);
        }
        else
        {
            drawStatusLine();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

bool CChartPane::draw(Store* store, std::string_view store_error, ImGuiID dock_id)
{
    if (focus_on_appear_)
    {
        ImGui::SetNextWindowFocus();
        focus_on_appear_ = false;
    }
    if (dock_id != 0)
    {
        ImGui::SetNextWindowDockID(dock_id, ImGuiCond_FirstUseEver);
    }

    char title[96];
    if (settings_.symbol.empty())
    {
        std::snprintf(title, sizeof(title), "CHART %d###chart_%d", id_, id_);
    }
    else
    {
        std::snprintf(title, sizeof(title), "%s  %s###chart_%d", settings_.symbol.c_str(),
                      chartPeriodCode(settings_.period), id_);
    }

    if (!ImGui::Begin(title, &window_open_))
    {
        ImGui::End();
        return false;
    }

    ImGui::BeginDisabled(studies_open_);
    if (ImGui::Button("Settings"))
    {
        openSettings();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(settings_open_);
    if (ImGui::Button("Studies"))
    {
        openStudies();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    drawStatusLine();
    for (const CStudyInstance& inst : studies_)
    {
        if (!inst.enabled)
        {
            continue;
        }
        const std::string label = studyShortLabel(inst);
        if (label.empty())
        {
            continue;
        }
        ImGui::SameLine();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(inst.color), "%s", label.c_str());
    }

    drawSettingsPopup(store, store_error);
    drawStudiesPopup();
    handleChartKeys();

    if (!settings_.symbol.empty())
    {
        const auto now = std::chrono::steady_clock::now();
        if (now - last_reload_ >= kReloadInterval)
        {
            reload(store, store_error);
        }
    }

    drawPlotBody();

    const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
    ImGui::End();
    return focused;
}

}  // namespace terminal
