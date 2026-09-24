// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/CChartPane.h"

#include "chart/CChartPlot.h"
#include "chart/CChartTransform.h"
#include "chart/CStudyCompute.h"
#include "chart/CStudySettings.h"
#include "data/IngestWorker.h"
#include "ui/Theme.h"

#include "market_data/Time.h"

#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <exception>
#include <string>

namespace terminal {
namespace {

constexpr auto kReloadInterval = std::chrono::seconds(2);
constexpr auto kChartKeyBufferTimeout = std::chrono::seconds(30);
constexpr std::size_t kChartKeyBufferMax = 32;

[[nodiscard]] std::string chartDownloadingMessage(const ChartDownloadRequest& request)
{
    const char* timeframe = request.timeframe_s == kTimeframe1d ? "1d" : "1m";
    return request.symbol + "  downloading " + timeframe + "  " + formatSessionDate(request.from) +
           " .. " + formatSessionDate(request.to);
}

void formatChartTitle(char* title, std::size_t title_n, int runtime_id, int id,
                      const CChartSettings& settings, std::string_view typed)
{
    const char* period = chartPeriodCode(settings.period);
    const int typed_n = static_cast<int>(typed.size());
    if (settings.symbol.empty())
    {
        if (typed.empty() && settings.period == ChartBarPeriod::Minute1)
        {
            std::snprintf(title, title_n, "CHART %d###cb%d_pane%d", id, runtime_id, id);
            return;
        }
        if (typed.empty())
        {
            std::snprintf(title, title_n, "CHART %d  %s###cb%d_pane%d", id, period, runtime_id, id);
            return;
        }
        if (settings.period == ChartBarPeriod::Minute1)
        {
            // %.*s uses typed_n as the length, so the view does not need a terminator.
            std::snprintf(title, title_n, "CHART %d  %.*s###cb%d_pane%d", id, typed_n,
                          typed.data(), // NOLINT(bugprone-suspicious-stringview-data-usage)
                          runtime_id, id);
            return;
        }
        std::snprintf(title, title_n, "CHART %d  %s  %.*s###cb%d_pane%d", id, period, typed_n,
                      typed.data(), // NOLINT(bugprone-suspicious-stringview-data-usage)
                      runtime_id, id);
        return;
    }
    if (typed.empty())
    {
        std::snprintf(title, title_n, "%s  %s###cb%d_pane%d", settings.symbol.c_str(), period, runtime_id, id);
        return;
    }
    std::snprintf(title, title_n, "%s  %s  %.*s###cb%d_pane%d", settings.symbol.c_str(), period, typed_n,
                  typed.data(), // NOLINT(bugprone-suspicious-stringview-data-usage)
                  runtime_id, id);
}

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

// Drawn on the candles. A one-pixel shadow keeps the glyphs readable on an up bar.
void drawShadowedText(std::string_view text, const ImVec4& color)
{
    if (text.empty())
    {
        return;
    }
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImU32 shadow = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.85f));
    ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x + 1.0f, pos.y + 1.0f), shadow, begin, end);
    const auto length = static_cast<int>(text.size());
    ImGui::TextColored(color, "%.*s", length,
                       begin); // NOLINT(bugprone-suspicious-stringview-data-usage)
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

std::string_view CChartPane::statusLine() const noexcept
{
    if (!loaded_.message.empty())
    {
        return loaded_.message;
    }
    switch (loaded_.status)
    {
    case ChartLoadStatus::Unconfigured:
        return "Type a symbol and press Enter, or open Chart Settings.";
    case ChartLoadStatus::Busy:
        return "store busy";
    case ChartLoadStatus::Ready:
    case ChartLoadStatus::Empty:
    case ChartLoadStatus::UnknownSymbol:
    case ChartLoadStatus::Unsupported:
    case ChartLoadStatus::Error:
        return {};
    }
    return {};
}

std::string_view CChartPane::keyNote() const noexcept
{
    return key_note_;
}

int CChartPane::barCount() const noexcept
{
    return static_cast<int>(loaded_.bars.size());
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
        clampStudyOptions(inst);
        normalizeStudyOutputs(inst);
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
    adoptResolvedIdentity(incoming);
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
                            chartSessionCount(loaded_settings_) != chartSessionCount(settings_);
    loaded_ = incoming;
    loaded_settings_ = settings_;
    if (reset_view)
    {
        view_.scroll_from_end = 0;
        resetChartScale(view_);
    }
    computed_ = studiesForLoad(loaded_, studies_);
}

void CChartPane::adoptResolvedIdentity(const ChartLoadResult& incoming)
{
    if (!incoming.instrument.has_value() || !incoming.instrument->figi.has_value())
    {
        return;
    }
    // The in-memory settings take the FIGI and the current ticker; the book picks both up on save.
    const Instrument& instrument = *incoming.instrument;
    settings_.figi = *instrument.figi;
    if (instrument.listing_open && normalizeChartSymbol(settings_.symbol) != instrument.symbol)
    {
        settings_.symbol = instrument.symbol;
        if (!settings_open_)
        {
            draft_.symbol = settings_.symbol;
            std::snprintf(draft_symbol_, sizeof(draft_symbol_), "%s", draft_.symbol.c_str());
        }
    }
    draft_.figi = settings_.figi;
}

void CChartPane::applyDraft(Store* store, std::string_view store_error, IngestWorker* ingest)
{
    draft_.symbol = normalizeChartSymbol(draft_symbol_);
    std::snprintf(draft_symbol_, sizeof(draft_symbol_), "%s", draft_.symbol.c_str());
    if (draft_.symbol != normalizeChartSymbol(settings_.symbol))
    {
        draft_.figi.clear();
    }
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
    applyLiveSettings(store, store_error, ingest);
}

void CChartPane::applyLiveSettings(Store* store, std::string_view store_error, IngestWorker* ingest)
{
    split_sync_symbol_.clear();
    split_sync_pending_.clear();
    split_sync_serial_ = 0;
    clampV1Limits(settings_);
    const bool same_series = loaded_settings_.symbol == settings_.symbol &&
                             loaded_settings_.period == settings_.period;
    if (download_serial_ == 0 || !same_series)
    {
        download_error_.clear();
        download_serial_ = 0;
        coverage_retry_ = false;
        const bool showing = loaded_.status == ChartLoadStatus::Ready && same_series;
        if (!showing)
        {
            requestMissingData(store, ingest);
        }
    }
    reload(store, store_error);
}

void CChartPane::requestMissingData(Store* store, IngestWorker* ingest)
{
    download_serial_ = 0;
    pending_download_ = {};
    coverage_retry_ = false;
    if (store == nullptr || normalizeChartSymbol(settings_.symbol).empty())
    {
        return;
    }

    ChartDownloadRequest window;
    try
    {
        const SessionDate today = utcToSessionDate("America/New_York", nowUtc());
        const std::optional<ChartDownloadRequest> needed =
            chartDownloadRequest(*store, settings_, today);
        if (!needed.has_value())
        {
            return;
        }
        window = needed.value();
    }
    catch (const std::exception& ex)
    {
        if (isStoreBusyError(ex.what()))
        {
            coverage_retry_ = true;
            return;
        }
        download_error_ = ex.what();
        return;
    }

    if (window.symbol.empty())
    {
        return;
    }
    pending_download_ = window;
    if (ingest == nullptr)
    {
        download_error_ = "ingest worker is not running";
        return;
    }
    IngestWorker::Job job;
    job.symbol = window.symbol;
    job.from = window.from;
    job.to = window.to;
    job.timeframe_s = window.timeframe_s;
    const IngestWorker::EnqueueResult result = ingest->enqueue(std::move(job));
    download_serial_ = result.serial;
}

void CChartPane::requestSplitSync(Store* store, std::string_view store_error, IngestWorker* ingest)
{
    if (split_sync_serial_ != 0 && ingest != nullptr)
    {
        const IngestWorker::Snapshot snap = ingest->snapshot();
        if (snap.finished_serial < split_sync_serial_)
        {
            return;
        }
        const std::uint64_t finished = split_sync_serial_;
        const std::string pending = split_sync_pending_;
        split_sync_serial_ = 0;
        split_sync_pending_.clear();
        const IngestWorker::SerialFailure failure = ingest->failureForSerial(finished);
        const std::string symbol = normalizeChartSymbol(settings_.symbol);
        const bool same_symbol = !pending.empty() && pending == symbol &&
                                 pending == normalizeChartSymbol(loaded_settings_.symbol);
        if (failure.failed)
        {
            if (same_symbol)
            {
                loaded_.message = failure.message.empty() ? "split sync failed" : failure.message;
                loaded_.status = ChartLoadStatus::Error;
            }
        }
        else if (same_symbol)
        {
            split_sync_symbol_ = pending;
            reload(store, store_error);
        }
    }

    if (store == nullptr || ingest == nullptr || settings_.period != ChartBarPeriod::Day1 ||
        loaded_.status != ChartLoadStatus::Ready || split_sync_serial_ != 0)
    {
        return;
    }
    const std::string symbol = normalizeChartSymbol(settings_.symbol);
    if (symbol.empty() || symbol != normalizeChartSymbol(loaded_settings_.symbol) ||
        symbol == split_sync_symbol_)
    {
        return;
    }

    IngestWorker::Job job;
    job.symbol = symbol;
    job.timeframe_s = kTimeframe1d;
    job.splits_only = true;
    const IngestWorker::EnqueueResult result = ingest->enqueue(std::move(job));
    split_sync_pending_ = symbol;
    split_sync_serial_ = result.serial;
}

void CChartPane::overlayDownloadStatus(Store* store, std::string_view store_error, IngestWorker* ingest)
{
    if (download_serial_ != 0 && ingest != nullptr)
    {
        const IngestWorker::Snapshot snap = ingest->snapshot();
        if (snap.finished_serial >= download_serial_)
        {
            const std::uint64_t finished = download_serial_;
            const IngestWorker::SerialFailure failure = ingest->failureForSerial(finished);
            download_serial_ = 0;
            if (failure.failed)
            {
                download_error_ = failure.message.empty() ? "ingest failed" : failure.message;
            }
            else
            {
                download_error_.clear();
                reload(store, store_error);
                return;
            }
        }
    }
    if (!download_error_.empty())
    {
        loaded_.status = ChartLoadStatus::Error;
        loaded_.message = download_error_;
        return;
    }
    if (download_serial_ == 0)
    {
        return;
    }
    loaded_.message = chartDownloadingMessage(pending_download_);
    if (!loaded_.bars.empty())
    {
        loaded_.status = ChartLoadStatus::Ready;
    }
    else if (loaded_.status != ChartLoadStatus::Ready)
    {
        loaded_.status = ChartLoadStatus::Empty;
    }
}

void CChartPane::drawSettingsPopup(Store* store, std::string_view store_error, IngestWorker* ingest)
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
                  ChartBarPeriod::Hour1, ChartBarPeriod::Day1,})
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

        ImGui::TextUnformatted("Intraday Days to Load");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        ImGui::InputInt("##intraday_days", &draft_.intraday_session_count);
        ImGui::TextUnformatted("Historical Days to Load");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        ImGui::InputInt("##historical_days", &draft_.historical_session_count);
        ImGui::TextColored(Theme::kMuted, "Intraday default 2 weeks. Historical default 5 years.");

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
            applyDraft(store, store_error, ingest);
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
    // A split list/settings layout needs a real size. AlwaysAutoResize collapses it.
    // Frame height tracks UI scale, so the window stays about 800×460 at 13 px type.
    const float row = ImGui::GetFrameHeight();
    ImGui::SetNextWindowSize(ImVec2(row * 42.0f, row * 24.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(row * 36.0f, row * 18.0f),
                                        ImVec2(row * 70.0f, row * 48.0f));
    if (ImGui::BeginPopupModal(popup_id, &studies_open_,
                               ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        const StudyDraftUi ui =
            drawStudyDraftBody(study_draft_, study_draft_selected_, next_study_id_);

        if (ui.length_enter || ui.apply || ui.ok)
        {
            applyStudyDraft();
        }
        if (ui.ok)
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
        if (ui.cancel || escape)
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

void CChartPane::drawOverlay()
{
    // SetCursorPos is from the window origin, and that origin includes the
    // title bar. Docked panes keep the same band for the tab strip. A small
    // y is clipped by the content rect and paints through the header.
    // CursorStartPos is already under that band; add the scroll so the value
    // is in SetCursorPos space.
    constexpr float kInset = 6.0f;
    const ImVec2 content(ImGui::GetCursorStartPos().x + ImGui::GetScrollX(),
                         ImGui::GetCursorStartPos().y + ImGui::GetScrollY());
    ImGui::SetCursorPos(ImVec2(content.x + kInset, content.y + kInset));

    // Default button fill is one step off the plot well, so the face disappears.
    ImGui::PushStyleColor(ImGuiCol_Button, Theme::kLine2);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::Mix(Theme::kLine2, Theme::kText, 0.22f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::kAccent);
    ImGui::PushStyleColor(ImGuiCol_Border, Theme::kTextDim);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
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
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    const std::string_view status = statusLine();
    if (!status.empty())
    {
        ImGui::SameLine();
        drawShadowedText(status, statusColor(loaded_.status));
    }
    if (!key_buffer_.empty())
    {
        ImGui::SameLine();
        drawShadowedText(key_buffer_, Theme::kAccent);
    }
    else if (!key_note_.empty())
    {
        ImGui::SameLine();
        drawShadowedText(key_note_, Theme::kDown);
    }
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
        drawShadowedText(label, ImGui::ColorConvertU32ToFloat4(studyPrimaryColor(inst)));
    }
}

void CChartPane::commitKeyBuffer(Store* store, std::string_view store_error, IngestWorker* ingest)
{
    const std::string text = key_buffer_;
    key_buffer_.clear();
    const ChartCommand command = parseChartCommand(text, settings_.period);
    if (command.kind == ChartCommandKind::Empty)
    {
        return;
    }
    if (command.kind == ChartCommandKind::Rejected)
    {
        key_note_ = command.message;
        return;
    }
    key_note_.clear();
    if (command.kind == ChartCommandKind::Symbol)
    {
        settings_.symbol = command.symbol;
        settings_.figi.clear();
    }
    else
    {
        settings_.period = command.period;
    }
    applyLiveSettings(store, store_error, ingest);
    // Reloading the plot can move the keyboard target off this pane.
    refocus_keyboard_ = true;
}

void CChartPane::handleChartKeys(Store* store, std::string_view store_error, IngestWorker* ingest)
{
    if (settings_open_ || studies_open_ || ImGui::GetIO().WantTextInput)
    {
        key_buffer_.clear();
        return;
    }
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
    {
        key_buffer_.clear();
        return;
    }
    // The scale menu and other popups own the keys while they are open.
    if (ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
    {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (!key_buffer_.empty() && now - key_buffer_at_ >= kChartKeyBufferTimeout)
    {
        key_buffer_.clear();
    }
    const ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        key_buffer_.clear();
        key_note_.clear();
    }
    else if (io.KeyCtrl || io.KeyAlt || io.KeySuper)
    {
        // Leave shortcuts alone.
    }
    else
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Backspace) && !key_buffer_.empty())
        {
            key_buffer_.pop_back();
            key_buffer_at_ = now;
            key_note_.clear();
        }
        for (const ImWchar ch : io.InputQueueCharacters)
        {
            if (ch > 127 || key_buffer_.size() >= kChartKeyBufferMax)
            {
                continue;
            }
            const auto c = static_cast<char>(ch);
            const auto u = static_cast<unsigned char>(c);
            if (std::isalnum(u) == 0 && c != '/' && c != '.' && c != '-' && c != ' ')
            {
                continue;
            }
            key_buffer_.push_back(c);
            key_buffer_at_ = now;
            key_note_.clear();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))
        {
            commitKeyBuffer(store, store_error, ingest);
        }
    }
    ImGui::SetNavCursorVisible(false);
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
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x > 0.0f && avail.y > 0.0f)
    {
        const ImU32 well = ImGui::ColorConvertFloat4ToU32(Theme::kBg0);
        ImGui::GetWindowDrawList()->AddRectFilled(origin,
                                                  ImVec2(origin.x + avail.x, origin.y + avail.y), well);
    }

    const bool draw_bars = !loaded_.bars.empty() && (loaded_.status == ChartLoadStatus::Ready ||
                                                     loaded_.status == ChartLoadStatus::Error);
    if (draw_bars)
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
        drawCandlesticks(loaded_.bars, settings_, view_, tz, computed_);
    }
    drawOverlay();
}

void CChartPane::setWindowScope(int runtime_id) noexcept
{
    runtime_id_ = runtime_id;
}

void CChartPane::setPlacement(bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size)
{
    place_force_ = force;
    place_floating_ = floating;
    place_dock_ = dock;
    place_pos_ = pos;
    place_size_ = size;
}

ChartbookPane CChartPane::exportRecord() const
{
    ChartbookPane record;
    record.id = id_;
    record.settings = settings_;
    record.interactive = view_.interactive;
    record.region_ratios = view_.region_ratios;
    record.next_study_id = next_study_id_;
    record.studies = studies_;
    return record;
}

void CChartPane::importRecord(const ChartbookPane& record)
{
    settings_ = record.settings;
    clampV1Limits(settings_);
    studies_ = record.studies;
    next_study_id_ = record.next_study_id > 0 ? record.next_study_id : 1;
    view_.interactive = record.interactive;
    view_.region_ratios = record.region_ratios;
    loaded_settings_ = {};
    loaded_ = {};
    computed_.clear();
}

bool CChartPane::draw(Store* store, std::string_view store_error, IngestWorker* ingest)
{
    if (focus_on_appear_ || refocus_keyboard_)
    {
        ImGui::SetNextWindowFocus();
        focus_on_appear_ = false;
        refocus_keyboard_ = false;
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
    formatChartTitle(title, sizeof(title), runtime_id_, id_, settings_, key_buffer_);

    // Enter and arrows are chart commands, not navigation between Settings and the plot.
    // No scrollbar: the series fills the client, and a bar would open a gap on the edge.
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (!ImGui::Begin(title, &window_open_, flags))
    {
        ImGui::End();
        ImGui::PopStyleVar();
        return false;
    }

    handleChartKeys(store, store_error, ingest);
    if (!settings_.symbol.empty())
    {
        const auto now = std::chrono::steady_clock::now();
        if (now - last_reload_ >= kReloadInterval)
        {
            reload(store, store_error);
            if (coverage_retry_ && download_serial_ == 0)
            {
                requestMissingData(store, ingest);
            }
        }
    }
    overlayDownloadStatus(store, store_error, ingest);
    requestSplitSync(store, store_error, ingest);
    overlayDownloadStatus(store, store_error, ingest);

    drawPlotBody();
    // After the overlay buttons, so a click opens the modal on this frame.
    drawSettingsPopup(store, store_error, ingest);
    drawStudiesPopup();

    const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
    ImGui::End();
    ImGui::PopStyleVar();
    return focused;
}

}  // namespace terminal
