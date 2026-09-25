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
#include <array>
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
                      const CChartSettings& settings, std::string_view typed, SymbolLinkGroup link)
{
    const char* period = chartPeriodCode(settings.period);
    const int typed_n = static_cast<int>(typed.size());
    char mark[8] = {};
    writeSymbolLinkMark(mark, sizeof(mark), link);
    if (settings.symbol.empty())
    {
        if (typed.empty() && settings.period == ChartBarPeriod::Minute1)
        {
            std::snprintf(title, title_n, "CHART %d%s###cb%d_pane%d", id, mark, runtime_id, id);
            return;
        }
        if (typed.empty())
        {
            std::snprintf(title, title_n, "CHART %d  %s%s###cb%d_pane%d", id, period, mark, runtime_id, id);
            return;
        }
        if (settings.period == ChartBarPeriod::Minute1)
        {
            // %.*s uses typed_n as the length, so the view does not need a terminator.
            std::snprintf(title, title_n, "CHART %d%s  %.*s###cb%d_pane%d", id, mark, typed_n,
                          typed.data(), // NOLINT(bugprone-suspicious-stringview-data-usage)
                          runtime_id, id);
            return;
        }
        std::snprintf(title, title_n, "CHART %d  %s%s  %.*s###cb%d_pane%d", id, period, mark, typed_n,
                      typed.data(), // NOLINT(bugprone-suspicious-stringview-data-usage)
                      runtime_id, id);
        return;
    }
    if (typed.empty())
    {
        std::snprintf(title, title_n, "%s  %s%s###cb%d_pane%d", settings.symbol.c_str(), period, mark, runtime_id,
                      id);
        return;
    }
    std::snprintf(title, title_n, "%s  %s%s  %.*s###cb%d_pane%d", settings.symbol.c_str(), period, mark, typed_n,
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

[[nodiscard]] float buttonWidth(const char* label)
{
    return ImGui::CalcTextSize(label).x + (ImGui::GetStyle().FramePadding.x * 2.0f);
}

[[nodiscard]] bool quietButton(const char* label)
{
    const ImVec4 clear(0.0f, 0.0f, 0.0f, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, clear);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::kBg3);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::kBg3);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    const bool pressed = ImGui::Button(label);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    return pressed;
}

[[nodiscard]] const char* scaleRangeName(ChartScaleRange range) noexcept
{
    switch (range)
    {
    case ChartScaleRange::Automatic:
        return "Automatic";
    case ChartScaleRange::ConstantRange:
        return "Constant Range";
    case ChartScaleRange::UserDefined:
        return "User Defined";
    }
    return "Automatic";
}

[[nodiscard]] const char* verticalGridLabel(ChartVerticalGrid grid) noexcept
{
    switch (grid)
    {
    case ChartVerticalGrid::Daily:
        return "Daily";
    case ChartVerticalGrid::Weekly:
        return "Weekly";
    case ChartVerticalGrid::Monthly:
        return "Monthly";
    }
    return "Daily";
}

[[nodiscard]] const char* horizontalGridLabel(ChartHorizontalGrid grid) noexcept
{
    switch (grid)
    {
    case ChartHorizontalGrid::Automatic:
        return "Automatic";
    case ChartHorizontalGrid::Manual:
        return "Manual";
    case ChartHorizontalGrid::Off:
        return "Off";
    }
    return "Automatic";
}

[[nodiscard]] const char* dragModeName(ChartInteractiveScale mode) noexcept
{
    switch (mode)
    {
    case ChartInteractiveScale::Range:
        return "Range";
    case ChartInteractiveScale::Move:
        return "Move";
    case ChartInteractiveScale::Locked:
        return "Locked";
    }
    return "Move";
}

void drawEmptyStatus(std::string_view text, ChartLoadStatus status)
{
    if (text.empty())
    {
        return;
    }
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x <= 1.0f || avail.y <= 1.0f)
    {
        return;
    }
    const bool downloading = text.find("downloading") != std::string_view::npos;
    const ImVec4 color = downloading ? Theme::kAccent : statusColor(status);
    constexpr float kPad = 12.0f;
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const float wrap_edge = origin.x + avail.x - kPad;
    float x = origin.x + kPad;
    float wrap_w = std::max(1.0f, wrap_edge - x);
    ImVec2 size = ImGui::CalcTextSize(begin, end, false, wrap_w);
    const float centered = origin.x + std::max(kPad, (avail.x - size.x) * 0.5f);
    if (centered > x)
    {
        // Centering moves the start, so the measured width is the gap to the same edge.
        x = centered;
        wrap_w = std::max(1.0f, wrap_edge - x);
        size = ImGui::CalcTextSize(begin, end, false, wrap_w);
    }
    const float y = origin.y + std::max(kPad, (avail.y - size.y) * 0.5f);
    const ImVec2 saved = ImGui::GetCursorPos();
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    // CalcWrapWidthForPos adds the window origin to a positive wrap position.
    const float wrap_local = wrap_edge - ImGui::GetWindowPos().x + ImGui::GetScrollX();
    ImGui::PushTextWrapPos(wrap_local);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(begin, end);
    ImGui::PopStyleColor();
    ImGui::PopTextWrapPos();
    ImGui::SetCursorPos(saved);
}

void drawSeriesStatusLine(std::string_view text, ChartLoadStatus status, ImVec2 origin, ImVec2 avail)
{
    if (text.empty() || avail.x <= 1.0f || avail.y <= 1.0f)
    {
        return;
    }
    const bool downloading = text.find("downloading") != std::string_view::npos;
    const ImVec4 color = downloading ? Theme::kAccent : statusColor(status);
    constexpr float kInset = 8.0f;
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    ImDrawList* list = ImGui::GetWindowDrawList();
    // The plot already filled the client. A text item would grow it.
    list->PushClipRect(origin, ImVec2(origin.x + avail.x, origin.y + avail.y), true);
    list->AddText(ImVec2(origin.x + kInset, origin.y + kInset), ImGui::GetColorU32(color), begin,
                  end);
    list->PopClipRect();
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
    draft_link_ = symbol_link_.group();
    std::snprintf(draft_symbol_, sizeof(draft_symbol_), "%s", draft_.symbol.c_str());
    settings_open_ = true;
}

void CChartPane::openStudies(int index)
{
    // Flag only. OpenPopup from the menu bar is a different ID stack than the pane.
    if (settings_open_)
    {
        return;
    }
    study_draft_ = studies_;
    const int count = static_cast<int>(study_draft_.size());
    if (count == 0)
    {
        study_draft_selected_ = -1;
    }
    else
    {
        if (index < 0 || index >= count)
        {
            index = 0;
        }
        study_draft_selected_ = index;
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
    const std::string previous = normalizeChartSymbol(settings_.symbol);
    const std::string committed = draft_.symbol;
    settings_ = draft_;
    symbol_link_.setGroup(draft_link_);
    applyLiveSettings(store, store_error, ingest);
    if (committed != previous)
    {
        link_publish_pending_ = true;
        link_publish_symbol_ = committed;
    }
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

        ImGui::TextUnformatted("Link");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140.0f);
        if (ImGui::BeginCombo("##chart_link", symbolLinkGroupLabel(draft_link_)))
        {
            constexpr SymbolLinkGroup kGroups[] = {
                SymbolLinkGroup::None,
                SymbolLinkGroup::One,
                SymbolLinkGroup::Two,
                SymbolLinkGroup::Three,
                SymbolLinkGroup::Four,
            };
            for (const SymbolLinkGroup group : kGroups)
            {
                if (ImGui::Selectable(symbolLinkGroupLabel(group), draft_link_ == group))
                {
                    draft_link_ = group;
                }
            }
            ImGui::EndCombo();
        }

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

        ImGui::Separator();
        ImGui::TextUnformatted("Vertical Grid");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::BeginCombo("##vertical_grid", verticalGridLabel(draft_.vertical_grid)))
        {
            for (const ChartVerticalGrid grid : {ChartVerticalGrid::Daily, ChartVerticalGrid::Weekly,
                                                 ChartVerticalGrid::Monthly,})
            {
                if (ImGui::Selectable(verticalGridLabel(grid), draft_.vertical_grid == grid))
                {
                    draft_.vertical_grid = grid;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::TextUnformatted("Horizontal Grid");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::BeginCombo("##horizontal_grid", horizontalGridLabel(draft_.horizontal_grid)))
        {
            for (const ChartHorizontalGrid grid : {ChartHorizontalGrid::Automatic, ChartHorizontalGrid::Manual,
                                                   ChartHorizontalGrid::Off,})
            {
                if (ImGui::Selectable(horizontalGridLabel(grid), draft_.horizontal_grid == grid))
                {
                    draft_.horizontal_grid = grid;
                }
            }
            ImGui::EndCombo();
        }
        if (draft_.horizontal_grid == ChartHorizontalGrid::Manual)
        {
            ImGui::TextUnformatted("Grid Spacing");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            ImGui::InputDouble("##h_grid", &draft_.horizontal_grid_spacing, 0.0, 0.0, "%.4f");
        }
        ImGui::TextColored(Theme::kMuted,
                           "Vertical labels are the day, week, or month. Spacing is a price increment.");

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

void CChartPane::showEnabledStudyTooltip() const
{
    bool open = false;
    const auto study_count = static_cast<int>(studies_.size());
    for (int index = 0; index < study_count; ++index)
    {
        const CStudyInstance& inst = studies_[static_cast<std::size_t>(index)];
        if (!inst.enabled)
        {
            continue;
        }
        const std::string label = studyShortLabel(inst);
        if (label.empty())
        {
            continue;
        }
        if (!open)
        {
            ImGui::BeginTooltip();
            open = true;
        }
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(studyPrimaryColor(inst)), "%s",
                           label.c_str());
    }
    if (open)
    {
        ImGui::EndTooltip();
    }
}

void CChartPane::drawStudyLegend(ImVec2 cursor, float width)
{
    if (!(width > 0.0f))
    {
        return;
    }
    struct Name
    {
        int index{0};
        std::string label;
        ImVec4 color;
        float width{0.0f};
    };
    std::array<Name, kStudyMaxPerPane> names{};
    int count = 0;
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float row_h = ImGui::GetFrameHeight();
    const auto study_count = static_cast<int>(studies_.size());
    for (int index = 0; index < study_count && count < kStudyMaxPerPane; ++index)
    {
        const CStudyInstance& inst = studies_[static_cast<std::size_t>(index)];
        if (!inst.enabled)
        {
            continue;
        }
        std::string label = studyShortLabel(inst);
        if (label.empty())
        {
            continue;
        }
        Name& name = names[static_cast<std::size_t>(count)];
        name.index = index;
        name.width = ImGui::CalcTextSize(label.c_str()).x;
        name.color = ImGui::ColorConvertU32ToFloat4(studyPrimaryColor(inst));
        name.label = std::move(label);
        ++count;
    }
    if (count == 0)
    {
        return;
    }

    const float ellipsis_w = ImGui::CalcTextSize("...").x;
    int fit = 0;
    float used = 0.0f;
    for (int i = 0; i < count; ++i)
    {
        const float gap_before = fit == 0 ? 0.0f : gap;
        const bool last = i + 1 == count;
        const float need = used + gap_before + names[static_cast<std::size_t>(i)].width;
        const float reserved = last ? need : (need + gap + ellipsis_w);
        const bool fits = reserved <= width;
        if (!fits)
        {
            break;
        }
        used = need;
        ++fit;
    }

    ImGui::SetCursorPos(cursor);
    const ImVec2 screen = ImGui::GetCursorScreenPos();
    ImGui::PushClipRect(screen, ImVec2(screen.x + width, screen.y + row_h), true);
    float x = cursor.x;
    for (int i = 0; i < fit; ++i)
    {
        if (i > 0)
        {
            x += gap;
        }
        const Name& name = names[static_cast<std::size_t>(i)];
        ImGui::SetCursorPos(ImVec2(x, cursor.y));
        ImGui::PushID(name.index);
        ImGui::InvisibleButton("##study_name", ImVec2(std::max(1.0f, name.width), row_h));
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        ImDrawList* list = ImGui::GetWindowDrawList();
        if (ImGui::IsItemHovered())
        {
            list->AddRectFilled(min, max, ImGui::GetColorU32(Theme::kBg3));
        }
        const float text_y = min.y + ((row_h - ImGui::GetTextLineHeight()) * 0.5f);
        list->AddText(ImVec2(min.x, text_y), ImGui::GetColorU32(name.color), name.label.c_str());
        if (ImGui::IsItemClicked())
        {
            openStudies(name.index);
        }
        ImGui::PopID();
        x += name.width;
    }
    if (fit < count)
    {
        const float mark_x = x + (fit > 0 ? gap : 0.0f);
        if (mark_x + ellipsis_w <= cursor.x + width)
        {
            ImGui::SetCursorPos(ImVec2(mark_x, cursor.y));
            const ImVec2 mark = ImGui::GetCursorScreenPos();
            const float text_y = mark.y + ((row_h - ImGui::GetTextLineHeight()) * 0.5f);
            ImGui::GetWindowDrawList()->AddText(ImVec2(mark.x, text_y),
                                                ImGui::GetColorU32(Theme::kTextDim), "...");
        }
    }
    ImGui::PopClipRect();

    // Under 8 px the strip itself owns the hover, so the two tips do not stack.
    constexpr float kLegendHoverMin = 8.0f;
    const bool popup = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
    if (width >= kLegendHoverMin && !popup && ImGui::IsWindowHovered() &&
        ImGui::IsMouseHoveringRect(screen, ImVec2(screen.x + width, screen.y + row_h)))
    {
        showEnabledStudyTooltip();
    }
}

void CChartPane::drawStrip(Store* store, std::string_view store_error, IngestWorker* ingest)
{
    // The dock tab bar belongs to the node, not this pane, and content in that
    // band is clipped. This row is the first line of the client, under the tab.
    const ImGuiStyle& style = ImGui::GetStyle();
    const float gap = style.ItemSpacing.x;
    const float row_h = ImGui::GetFrameHeight();
    const float width = std::max(0.0f, ImGui::GetContentRegionAvail().x);
    const float row_x = ImGui::GetCursorPosX();
    const float row_y = ImGui::GetCursorPosY();
    const ImVec2 origin = ImGui::GetCursorScreenPos();

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImU32 chrome = ImGui::GetColorU32(Theme::kBg1);
    draw->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + row_h), chrome);
    draw->AddLine(ImVec2(origin.x, origin.y + row_h), ImVec2(origin.x + width, origin.y + row_h),
                  ImGui::GetColorU32(Theme::kLine));

    const char* period_label = chartPeriodCode(settings_.period);
    const char* scale_label = chartScaleStripLabel(settings_.scale_range);
    const float right_w = buttonWidth(period_label) + gap + buttonWidth(scale_label);
    float period_x = row_x + width - right_w;
    float legend_w = period_x - gap - row_x;
    if (legend_w < 0.0f)
    {
        legend_w = 0.0f;
        period_x = row_x;
    }
    constexpr float kLegendHoverMin = 8.0f;
    if (legend_w > 0.0f)
    {
        drawStudyLegend(ImVec2(row_x, row_y), legend_w);
    }

    ImGui::SetCursorPos(ImVec2(period_x, row_y));
    ImGui::BeginDisabled(settings_open_);
    if (quietButton(period_label))
    {
        ImGui::OpenPopup("##strip_period");
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        ImGui::SetTooltip("%s", periodDisplayName(settings_.period));
    }
    if (ImGui::BeginPopup("##strip_period"))
    {
        for (const ChartBarPeriod period :
             {ChartBarPeriod::Minute1, ChartBarPeriod::Minute5, ChartBarPeriod::Minute15,
              ChartBarPeriod::Hour1, ChartBarPeriod::Day1,})
        {
            if (ImGui::MenuItem(periodDisplayName(period), nullptr, settings_.period == period) &&
                period != settings_.period)
            {
                settings_.period = period;
                applyLiveSettings(store, store_error, ingest);
            }
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(settings_open_ || loaded_.bars.empty());
    if (quietButton(scale_label))
    {
        ImGui::OpenPopup("##strip_scale");
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        ImGui::SetTooltip("Scale Range: %s / %s", scaleRangeName(settings_.scale_range),
                          dragModeName(view_.interactive));
    }

    const bool popup = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
    if (legend_w < kLegendHoverMin && !popup && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() &&
        ImGui::IsMouseHoveringRect(origin, ImVec2(origin.x + width, origin.y + row_h)))
    {
        showEnabledStudyTooltip();
    }

    ImGui::SetCursorPos(ImVec2(row_x, row_y + row_h + 1.0f));
}

void CChartPane::drawStripScalePopup()
{
    if (!ImGui::BeginPopup("##strip_scale"))
    {
        return;
    }
    drawChartScaleMenuItems(settings_, view_, view_.price_ylim, view_.price_ylim_valid);
    ImGui::EndPopup();
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
        const std::string committed = normalizeChartSymbol(command.symbol);
        const bool changed = commitSymbol(command.symbol);
        applyLiveSettings(store, store_error, ingest);
        if (changed)
        {
            link_publish_pending_ = true;
            link_publish_symbol_ = committed;
        }
    }
    else
    {
        settings_.period = command.period;
        applyLiveSettings(store, store_error, ingest);
    }
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
    if (ImGui::IsKeyPressed(ImGuiKey_F5))
    {
        key_buffer_.clear();
        openSettings();
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F6))
    {
        key_buffer_.clear();
        openStudies();
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
        // A failed reload keeps these bars. The rail only shows the focused chart.
        if (loaded_.status != ChartLoadStatus::Ready)
        {
            drawSeriesStatusLine(statusLine(), loaded_.status, origin, avail);
        }
    }
    else
    {
        drawEmptyStatus(statusLine(), loaded_.status);
    }
}

void CChartPane::setWindowScope(int runtime_id) noexcept
{
    runtime_id_ = runtime_id;
}

void CChartPane::attachSymbolLink(CSymbolLink& link)
{
    symbol_link_.attach(link, paneWindowId(id_), &CChartPane::applyLinkedThunk, this);
}

void CChartPane::setSymbolLinkGroup(int group) noexcept
{
    symbol_link_.setGroup(symbolLinkGroupFromInt(group));
}

bool CChartPane::commitSymbol(std::string_view raw)
{
    const std::string symbol = normalizeChartSymbol(raw);
    if (symbol == normalizeChartSymbol(settings_.symbol))
    {
        return false;
    }
    settings_.symbol = symbol;
    settings_.figi.clear();
    if (!settings_open_)
    {
        draft_.symbol = settings_.symbol;
        draft_.figi.clear();
        std::snprintf(draft_symbol_, sizeof(draft_symbol_), "%s", draft_.symbol.c_str());
    }
    return true;
}

void CChartPane::flushSymbolLink()
{
    if (!link_publish_pending_)
    {
        return;
    }
    link_publish_pending_ = false;
    symbol_link_.publish(link_publish_symbol_);
}

void CChartPane::applyLinkedThunk(void* self, std::string_view symbol)
{
    auto* pane = static_cast<CChartPane*>(self);
    if (pane->commitSymbol(symbol))
    {
        pane->linked_reload_ = true;
    }
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
    record.link_group = static_cast<int>(symbol_link_.group());
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

    if (linked_reload_)
    {
        linked_reload_ = false;
        applyLiveSettings(store, store_error, ingest);
    }

    char title[160];
    formatChartTitle(title, sizeof(title), runtime_id_, id_, settings_, key_buffer_, symbol_link_.group());

    // Enter and arrows are chart commands, not navigation between Settings and the plot.
    // No scrollbar: the series fills the client, and a bar would open a gap on the edge.
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (!ImGui::Begin(title, &window_open_, flags))
    {
        ImGui::End();
        ImGui::PopStyleVar();
        if (window_open_)
        {
            flushSymbolLink();
        }
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

    view_.price_ylim_valid = false;
    drawStrip(store, store_error, ingest);
    drawPlotBody();
    drawStripScalePopup();
    drawSettingsPopup(store, store_error, ingest);
    drawStudiesPopup();
    flushSymbolLink();

    const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
    ImGui::End();
    ImGui::PopStyleVar();
    return focused;
}

}  // namespace terminal
