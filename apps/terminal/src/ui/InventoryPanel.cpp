// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "ui/InventoryPanel.h"

#include "IngestDefaults.h"
#include "RepoRoot.h"
#include "data/IngestWorker.h"
#include "ui/ReceivedStamp.h"
#include "ui/Theme.h"

#include "market_data/NyseCalendar.h"
#include "market_data/Time.h"

#include "imgui_internal.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <stdexcept>
#include <string_view>

namespace terminal {
namespace {

constexpr int kColSymbol = 0;
constexpr int kColFigi = 1;
constexpr int kColTimeframe = 2;
constexpr int kColBars = 3;
constexpr int kColSessions = 4;
constexpr int kColComplete = 5;
constexpr int kColPartial = 6;
constexpr int kColMissing = 7;
constexpr int kColError = 8;
constexpr int kColFirst = 9;
constexpr int kColLast = 10;

[[nodiscard]] const char* timeframeLabel(int timeframe_s)
{
    if (timeframe_s == kTimeframe1d)
    {
        return "1d";
    }
    if (timeframe_s == kTimeframe1m)
    {
        return "1m";
    }
    return "";
}

[[nodiscard]] SessionDate defaultFromDate(SessionDate today, int timeframe_s)
{
    const auto ymd = sessionDateToYmd(today);
    const int lookback =
        timeframe_s == kTimeframe1d ? kIngestDefaultDailyDays : kIngestDefaultIntradayDays;
    return toSessionDate(std::chrono::sys_days{ymd} - std::chrono::days{lookback});
}

[[nodiscard]] ImVec4 statusColor(CoverageStatus status)
{
    switch (status)
    {
    case CoverageStatus::Complete:
        return Theme::kUp;
    case CoverageStatus::Partial:
        return Theme::kWarn;
    case CoverageStatus::Missing:
        return Theme::kMuted;
    case CoverageStatus::Error:
        return Theme::kDown;
    }
    return Theme::kMuted;
}

void cellRight(const char* text)
{
    const float width = ImGui::GetContentRegionAvail().x;
    const float text_w = ImGui::CalcTextSize(text).x;
    if (text_w < width)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + width - text_w);
    }
    ImGui::TextUnformatted(text);
}

[[nodiscard]] ImFont* pushMono()
{
    ImFont* const mono = Theme::monoFont();
    if (mono != nullptr)
    {
        ImGui::PushFont(mono);
    }
    return mono;
}

void cellMono(const char* text)
{
    const ImFont* const mono = pushMono();
    ImGui::TextUnformatted(text);
    if (mono != nullptr)
    {
        ImGui::PopFont();
    }
}

void cellInt(int value)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d", value);
    const ImFont* const mono = pushMono();
    cellRight(buf);
    if (mono != nullptr)
    {
        ImGui::PopFont();
    }
}

void dimLabel(const char* text)
{
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(Theme::kTextDim, "%s", text);
}

[[nodiscard]] std::string formatUtcMinute(UnixSeconds ts)
{
    const auto t = static_cast<std::time_t>(ts);
    std::tm utc{};
    if (!tryUtcTm(t, utc))
    {
        return {};
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d", utc.tm_year + 1900, utc.tm_mon + 1,
                  utc.tm_mday, utc.tm_hour, utc.tm_min);
    return buf;
}

void uppercaseInPlace(char* text)
{
    for (char* p = text; *p != '\0'; ++p)
    {
        *p = static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
    }
}

int compareOptionalDate(const std::optional<SessionDate>& a, const std::optional<SessionDate>& b)
{
    const SessionDate av = a.value_or(0);
    const SessionDate bv = b.value_or(0);
    if (av < bv)
    {
        return -1;
    }
    if (av > bv)
    {
        return 1;
    }
    return 0;
}

}  // namespace

InventoryPanel::InventoryPanel() : db_path_(defaultMarketDataDbPath())
{
    try
    {
        std::filesystem::create_directories(db_path_.parent_path());
        {
            const Store migrate(db_path_, StoreMode::Writer);
            (void)migrate.userVersion();
        }
        store_ = std::make_unique<Store>(db_path_, StoreMode::Reader);
        worker_ = std::make_unique<IngestWorker>(db_path_, defaultSecretsPath());
        fillDefaultDates();
        refreshSummaries();
        status_ = summaries_.empty() ? "no coverage yet" : "idle";
        last_refresh_ = std::chrono::steady_clock::now();
    }
    catch (const std::exception& ex)
    {
        open_error_ = ex.what();
        status_ = open_error_;
    }
}

InventoryPanel::~InventoryPanel() = default;

IngestWorker* InventoryPanel::ingestWorker() noexcept
{
    return worker_.get();
}

const IngestWorker* InventoryPanel::ingestWorker() const noexcept
{
    return worker_.get();
}

std::string_view InventoryPanel::statusText() const noexcept
{
    return status_;
}

std::string_view InventoryPanel::openError() const noexcept
{
    return open_error_;
}

void InventoryPanel::fillDefaultDates()
{
    const SessionDate today = utcToSessionDate("America/New_York", nowUtc());
    const SessionDate from = defaultFromDate(today, ingest_timeframe_s_);
    std::snprintf(from_, sizeof(from_), "%08d", static_cast<int>(from));
    std::snprintf(to_, sizeof(to_), "%08d", static_cast<int>(today));
}

void InventoryPanel::pollWorker()
{
    if (worker_ == nullptr)
    {
        return;
    }
    const auto snap = worker_->snapshot();
    if (!snap.error.empty())
    {
        status_ = snap.error;
    }
    else if (!snap.message.empty())
    {
        status_ = snap.message;
        if (snap.queued > 0)
        {
            status_ += " · queued ";
            status_ += std::to_string(snap.queued);
        }
    }
    const auto now = std::chrono::steady_clock::now();
    const auto interval =
        snap.running ? std::chrono::milliseconds(400) : std::chrono::seconds(2);
    if (snap.dirty || now - last_refresh_ >= interval)
    {
        if (snap.dirty)
        {
            worker_->clearDirty();
        }
        refreshSummaries();
        refreshDays();
        last_refresh_ = now;
    }
}

void InventoryPanel::refreshSummaries()
{
    if (store_ == nullptr)
    {
        return;
    }
    try
    {
        summaries_ = store_->queryCoverageSummaries(kTimeframe1m);
        const auto daily = store_->queryCoverageSummaries(kTimeframe1d);
        for (const CoverageSummary& row : daily)
        {
            if (row.session_count > 0 || row.bar_count > 0)
            {
                summaries_.push_back(row);
            }
        }
    }
    catch (const std::exception& ex)
    {
        const std::string_view what = ex.what();
        if (what.find("busy") == std::string_view::npos)
        {
            status_ = ex.what();
        }
    }
}

void InventoryPanel::refreshDays()
{
    if (store_ == nullptr || !selected_id_.has_value())
    {
        days_.clear();
        return;
    }
    try
    {
        days_ = store_->queryCoverageDays(selected_id_.value_or(0), selected_timeframe_s_);
        selected_history_.clear();
        for (const InstrumentListing& listing : store_->listingHistory(selected_id_.value_or(0)))
        {
            if (!listing.closed_at.has_value())
            {
                continue;
            }
            const SessionDate until = utcToSessionDate("America/New_York", *listing.closed_at);
            selected_history_ += selected_history_.empty() ? "formerly " : ", ";
            selected_history_ += listing.symbol + " until " + formatSessionDate(until);
        }
    }
    catch (const std::exception& ex)
    {
        const std::string_view what = ex.what();
        if (what.find("busy") == std::string_view::npos)
        {
            status_ = ex.what();
        }
    }
}

bool InventoryPanel::focused() const noexcept
{
    return focused_;
}

std::optional<UnixSeconds> InventoryPanel::receivedAt() const
{
    std::optional<UnixSeconds> latest;
    for (const CoverageSummary& row : summaries_)
    {
        if (!row.last_ingested_at.has_value())
        {
            continue;
        }
        if (!latest.has_value() || *row.last_ingested_at > *latest)
        {
            latest = *row.last_ingested_at;
        }
    }
    return latest;
}

void InventoryPanel::requestData()
{
    submitIngest();
}

void InventoryPanel::submitIngest()
{
    uppercaseInPlace(symbol_);
    if (symbol_[0] == '\0')
    {
        status_ = "enter a symbol";
        return;
    }
    if (worker_ == nullptr)
    {
        status_ = open_error_.empty() ? "ingest worker is not running" : open_error_;
        return;
    }
    try
    {
        const SessionDate from = parseSessionDate(from_);
        const SessionDate to = parseSessionDate(to_);
        if (from > to)
        {
            status_ = "FROM must be on or before TO";
            return;
        }
        worker_->enqueue(IngestWorker::Job{.symbol=symbol_, .from=from, .to=to, .timeframe_s=ingest_timeframe_s_});
        status_ = std::string("queued ") + symbol_ + " " + formatSessionDate(from) + ".." +
                  formatSessionDate(to);
    }
    catch (const std::exception& ex)
    {
        status_ = ex.what();
    }
}

void InventoryPanel::setWindowScope(int runtime_id) noexcept
{
    runtime_id_ = runtime_id;
}

void InventoryPanel::setPlacement(bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size)
{
    place_force_ = force;
    place_floating_ = floating;
    place_dock_ = dock;
    place_pos_ = pos;
    place_size_ = size;
}

ChartbookData InventoryPanel::exportData() const
{
    ChartbookData data;
    data.symbol = symbol_;
    data.from = from_;
    data.to = to_;
    data.ingest_timeframe = ingest_timeframe_s_ == kTimeframe1d ? "1d" : "1m";
    data.selected_symbol = selected_symbol_;
    data.selected_timeframe = selected_timeframe_;
    data.sort_column = sort_column_;
    data.sort_descending = sort_descending_;
    data.columns = columns_;
    return data;
}

void InventoryPanel::importData(const ChartbookData& data)
{
    std::snprintf(symbol_, sizeof(symbol_), "%s", data.symbol.c_str());
    ingest_timeframe_s_ = data.ingest_timeframe == "1d" ? kTimeframe1d : kTimeframe1m;
    if (data.from.empty() || data.to.empty())
    {
        fillDefaultDates();
    }
    else
    {
        std::snprintf(from_, sizeof(from_), "%s", data.from.c_str());
        std::snprintf(to_, sizeof(to_), "%s", data.to.c_str());
    }
    selected_symbol_ = data.selected_symbol;
    selected_timeframe_ = data.selected_timeframe;
    pending_symbol_ = data.selected_symbol;
    pending_timeframe_ = data.selected_timeframe;
    selected_id_.reset();
    columns_ = data.columns;
    sort_column_ = data.sort_column;
    sort_descending_ = data.sort_descending;
    apply_columns_ = true;
    ignore_settings_dirty_ = true;
    resolveSelection();
}

void InventoryPanel::resolveSelection()
{
    if (pending_symbol_.empty())
    {
        return;
    }
    const int timeframe_s = pending_timeframe_ == "1d" ? kTimeframe1d : kTimeframe1m;
    for (const CoverageSummary& row : summaries_)
    {
        if (row.instrument.symbol == pending_symbol_ && row.timeframe_s == timeframe_s)
        {
            selected_id_ = row.instrument.id;
            selected_timeframe_s_ = row.timeframe_s;
            selected_symbol_ = row.instrument.symbol;
            selected_timeframe_ = pending_timeframe_.empty() ? "1m" : pending_timeframe_;
            pending_symbol_.clear();
            pending_timeframe_.clear();
            return;
        }
    }
}

void InventoryPanel::applySavedColumns()
{
    ImGuiTable* table = ImGui::GetCurrentTable();
    if (table == nullptr)
    {
        return;
    }
    for (int column_n = 0; column_n < table->ColumnsCount; ++column_n)
    {
        ImGuiTableColumn& column = table->Columns[column_n];
        column.SortOrder = -1;
        column.SortDirection = ImGuiSortDirection_None;
    }
    if (!columns_.empty())
    {
        std::vector<int> orders;
        const int column_count = std::min(table->ColumnsCount, kChartbookDataColumnCount);
        const bool apply_order = chartbookColumnDisplayOrders(columns_, column_count, orders) &&
                                 column_count == table->ColumnsCount;
        for (int column_n = 0; column_n < column_count; ++column_n)
        {
            const std::string id = kChartbookDataColumns[column_n];
            const auto found = std::ranges::find_if(columns_, [&](const ChartbookColumn& column) {
                return column.id == id;
            });
            if (found == columns_.end())
            {
                continue;
            }
            ImGuiTableColumn& column = table->Columns[column_n];
            column.IsUserEnabled = found->visible;
            column.IsUserEnabledNextFrame = found->visible;
            if (apply_order)
            {
                column.DisplayOrder = static_cast<ImGuiTableColumnIdx>(orders[static_cast<std::size_t>(column_n)]);
            }
            if (found->width > 1.f)
            {
                column.WidthRequest = found->width;
                column.WidthGiven = found->width;
            }
        }
        if (apply_order)
        {
            for (int column_n = 0; column_n < table->ColumnsCount; ++column_n)
            {
                const int order = table->Columns[column_n].DisplayOrder;
                if (order >= 0 && order < table->ColumnsCount)
                {
                    table->DisplayOrderToIndex[order] = static_cast<ImGuiTableColumnIdx>(column_n);
                }
            }
        }
    }
    if (!sort_column_.empty())
    {
        for (int column_n = 0; column_n < table->ColumnsCount && column_n < kChartbookDataColumnCount; ++column_n)
        {
            if (sort_column_ != kChartbookDataColumns[column_n])
            {
                continue;
            }
            ImGuiTableColumn& column = table->Columns[column_n];
            column.SortOrder = 0;
            column.SortDirection =
                sort_descending_ ? ImGuiSortDirection_Descending : ImGuiSortDirection_Ascending;
        }
    }
    table->IsSortSpecsDirty = true;
}

void InventoryPanel::snapshotColumns()
{
    ImGuiTable* table = ImGui::GetCurrentTable();
    if (table == nullptr)
    {
        return;
    }
    columns_.clear();
    sort_column_.clear();
    sort_descending_ = false;
    for (int column_n = 0; column_n < table->ColumnsCount && column_n < kChartbookDataColumnCount; ++column_n)
    {
        const ImGuiTableColumn& column = table->Columns[column_n];
        ChartbookColumn saved;
        saved.id = kChartbookDataColumns[column_n];
        saved.width = column.WidthGiven;
        saved.visible = column.IsUserEnabled;
        saved.order = column.DisplayOrder;
        columns_.push_back(std::move(saved));
        if (column.SortOrder == 0 && column.SortDirection != ImGuiSortDirection_None)
        {
            sort_column_ = kChartbookDataColumns[column_n];
            sort_descending_ = column.SortDirection == ImGuiSortDirection_Descending;
        }
    }
}

void InventoryPanel::noteColumnEdits()
{
    ImGuiTable* table = ImGui::GetCurrentTable();
    if (table == nullptr)
    {
        return;
    }
    if (ignore_settings_dirty_)
    {
        ignore_settings_dirty_ = false;
        table->IsSettingsDirty = false;
        return;
    }
    if (table->IsSettingsDirty)
    {
        snapshotColumns();
        table->IsSettingsDirty = false;
    }
}

bool InventoryPanel::draw()
{
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

    char title[64];
    std::snprintf(title, sizeof(title), "DATA###cb%d_data", runtime_id_);
    bool open = true;
    if (!ImGui::Begin(title, &open, ImGuiWindowFlags_NoSavedSettings))
    {
        focused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        ImGui::End();
        return open;
    }

    if (!open_error_.empty() && store_ == nullptr)
    {
        ImGui::TextColored(Theme::kDown, "%s", open_error_.c_str());
        focused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        ImGui::End();
        return open;
    }

    pollWorker();
    resolveSelection();
    drawToolbar();
    ImGui::Separator();
    ImGui::TextColored(Theme::kMuted, "%s", status_.c_str());
    drawReceivedStamp(receivedAt());

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float summary_h = avail.y * 0.58f;
    if (ImGui::BeginChild("summaries", ImVec2(0.0f, summary_h), ImGuiChildFlags_Borders))
    {
        drawSummaryTable();
    }
    ImGui::EndChild();
    if (ImGui::BeginChild("days", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders))
    {
        drawDayTable();
    }
    ImGui::EndChild();
    focused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
    ImGui::End();
    return open;
}

void InventoryPanel::drawToolbar()
{
    ImGui::PushStyleColor(ImGuiCol_FrameBg, Theme::kField);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Theme::kBg3);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Theme::kBg3);

    dimLabel("Symbol");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(72.0f);
    const bool symbol_go =
        ImGui::InputText("##symbol", symbol_, sizeof(symbol_),
                         ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    dimLabel("Tf");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(52.0f);
    const char* tf_label = ingest_timeframe_s_ == kTimeframe1d ? "1d" : "1m";
    if (ImGui::BeginCombo("##ingest_tf", tf_label))
    {
        const bool sel_1m = ingest_timeframe_s_ == kTimeframe1m;
        if (ImGui::Selectable("1m", sel_1m) && ingest_timeframe_s_ != kTimeframe1m)
        {
            const SessionDate today = utcToSessionDate("America/New_York", nowUtc());
            char old_default[16]{};
            std::snprintf(old_default, sizeof(old_default), "%08d",
                          static_cast<int>(defaultFromDate(today, ingest_timeframe_s_)));
            ingest_timeframe_s_ = kTimeframe1m;
            if (std::strcmp(from_, old_default) == 0)
            {
                std::snprintf(from_, sizeof(from_), "%08d",
                              static_cast<int>(defaultFromDate(today, ingest_timeframe_s_)));
            }
        }
        const bool sel_1d = ingest_timeframe_s_ == kTimeframe1d;
        if (ImGui::Selectable("1d", sel_1d) && ingest_timeframe_s_ != kTimeframe1d)
        {
            const SessionDate today = utcToSessionDate("America/New_York", nowUtc());
            char old_default[16]{};
            std::snprintf(old_default, sizeof(old_default), "%08d",
                          static_cast<int>(defaultFromDate(today, ingest_timeframe_s_)));
            ingest_timeframe_s_ = kTimeframe1d;
            if (std::strcmp(from_, old_default) == 0)
            {
                std::snprintf(from_, sizeof(from_), "%08d",
                              static_cast<int>(defaultFromDate(today, ingest_timeframe_s_)));
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    dimLabel("From");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(88.0f);
    const bool from_go = ImGui::InputText("##from", from_, sizeof(from_),
                                          ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    dimLabel("To");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(88.0f);
    const bool to_go =
        ImGui::InputText("##to", to_, sizeof(to_), ImGuiInputTextFlags_EnterReturnsTrue);

    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, Theme::kGo);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::kAccentHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::kAccentPressed);
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kBg0);
    const bool clicked = ImGui::Button("GO");
    ImGui::PopStyleColor(4);

    if (symbol_go || from_go || to_go || clicked)
    {
        submitIngest();
    }
}

void InventoryPanel::applySortSpecs()
{
    ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs();
    if (specs == nullptr || specs->SpecsCount == 0)
    {
        return;
    }
    const ImGuiTableColumnSortSpecs& spec = specs->Specs[0];
    const int col = spec.ColumnIndex;
    const bool desc = spec.SortDirection == ImGuiSortDirection_Descending;
    std::ranges::sort(summaries_, [&](const CoverageSummary& a, const CoverageSummary& b) {
        int delta = 0;
        switch (col)
        {
        case kColSymbol:
            delta = a.instrument.symbol.compare(b.instrument.symbol);
            break;
        case kColFigi:
            delta = a.instrument.figi.value_or("").compare(b.instrument.figi.value_or(""));
            break;
        case kColTimeframe:
            delta = a.timeframe_s - b.timeframe_s;
            break;
        case kColBars:
            delta = a.bar_count - b.bar_count;
            break;
        case kColSessions:
            delta = a.session_count - b.session_count;
            break;
        case kColComplete:
            delta = a.complete_count - b.complete_count;
            break;
        case kColPartial:
            delta = a.partial_count - b.partial_count;
            break;
        case kColMissing:
            delta = a.missing_count - b.missing_count;
            break;
        case kColError:
            delta = a.error_count - b.error_count;
            break;
        case kColFirst:
            delta = compareOptionalDate(a.first_session, b.first_session);
            break;
        case kColLast:
            delta = compareOptionalDate(a.last_session, b.last_session);
            break;
        default:
            break;
        }
        if (delta == 0)
        {
            delta = a.timeframe_s - b.timeframe_s;
        }
        if (delta == 0)
        {
            delta = a.instrument.symbol.compare(b.instrument.symbol);
        }
        return desc ? delta > 0 : delta < 0;
    });
    specs->SpecsDirty = false;
}

void InventoryPanel::drawSummaryTable()
{
    constexpr ImGuiTableFlags flags =
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable |
        ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp |
        ImGuiTableFlags_SortTristate;

    if (!ImGui::BeginTable("inventory", 11, flags, ImVec2(0.0f, 0.0f)))
    {
        return;
    }
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("SYMBOL", ImGuiTableColumnFlags_DefaultSort);
    ImGui::TableSetupColumn("FIGI", ImGuiTableColumnFlags_WidthFixed, 96.0f);
    ImGui::TableSetupColumn("TF", ImGuiTableColumnFlags_WidthFixed, 36.0f);
    ImGui::TableSetupColumn("BARS", ImGuiTableColumnFlags_WidthFixed, 64.0f);
    ImGui::TableSetupColumn("Sessions", ImGuiTableColumnFlags_WidthFixed, 72.0f);
    ImGui::TableSetupColumn("OK", ImGuiTableColumnFlags_WidthFixed, 40.0f);
    ImGui::TableSetupColumn("Partial", ImGuiTableColumnFlags_WidthFixed, 64.0f);
    ImGui::TableSetupColumn("Missing", ImGuiTableColumnFlags_WidthFixed, 68.0f);
    ImGui::TableSetupColumn("ERR", ImGuiTableColumnFlags_WidthFixed, 40.0f);
    ImGui::TableSetupColumn("FIRST");
    ImGui::TableSetupColumn("LAST");
    if (apply_columns_)
    {
        applySavedColumns();
        apply_columns_ = false;
    }
    ImGui::TableHeadersRow();
    applySortSpecs();

    if (summaries_.empty())
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(Theme::kMuted, "No names yet. Enter a symbol and GO.");
        noteColumnEdits();
        ImGui::EndTable();
        return;
    }

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(summaries_.size()));
    while (clipper.Step())
    {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
        {
            const CoverageSummary& row = summaries_[static_cast<std::size_t>(i)];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(kColSymbol);
            const bool selected = selected_id_.value_or(-1) == row.instrument.id &&
                                  selected_timeframe_s_ == row.timeframe_s;
            char label[80];
            std::snprintf(label, sizeof(label), "%s##%lld-%d", row.instrument.symbol.c_str(),
                          static_cast<long long>(row.instrument.id), row.timeframe_s);
            if (ImGui::Selectable(label, selected,
                                  ImGuiSelectableFlags_SpanAllColumns |
                                      ImGuiSelectableFlags_AllowOverlap))
            {
                selected_id_ = row.instrument.id;
                selected_timeframe_s_ = row.timeframe_s;
                selected_symbol_ = row.instrument.symbol;
                selected_timeframe_ = timeframeLabel(row.timeframe_s);
                pending_symbol_.clear();
                pending_timeframe_.clear();
                ingest_timeframe_s_ = row.timeframe_s;
                std::snprintf(symbol_, sizeof(symbol_), "%s", row.instrument.symbol.c_str());
                refreshDays();
            }
            ImGui::TableNextColumn();
            const std::string figi = row.instrument.figi.value_or("");
            ImGui::TextUnformatted(figi.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(timeframeLabel(row.timeframe_s));
            ImGui::TableNextColumn();
            cellInt(row.bar_count);
            ImGui::TableNextColumn();
            cellInt(row.session_count);
            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, row.complete_count > 0 ? Theme::kUp : Theme::kMuted);
            cellInt(row.complete_count);
            ImGui::PopStyleColor();
            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  row.partial_count > 0 ? Theme::kWarn : Theme::kMuted);
            cellInt(row.partial_count);
            ImGui::PopStyleColor();
            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, row.missing_count > 0 ? Theme::kWarn : Theme::kMuted);
            cellInt(row.missing_count);
            ImGui::PopStyleColor();
            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, row.error_count > 0 ? Theme::kDown : Theme::kMuted);
            cellInt(row.error_count);
            ImGui::PopStyleColor();
            ImGui::TableNextColumn();
            const SessionDate first_date = row.first_session.value_or(0);
            const std::string first =
                first_date != 0 ? formatSessionDate(first_date) : std::string{};
            cellMono(first.c_str());
            ImGui::TableNextColumn();
            const SessionDate last_date = row.last_session.value_or(0);
            const std::string last =
                last_date != 0 ? formatSessionDate(last_date) : std::string{};
            cellMono(last.c_str());
        }
    }
    noteColumnEdits();
    ImGui::EndTable();
}

void InventoryPanel::drawDayTable()
{
    if (!selected_id_.has_value())
    {
        ImGui::TextColored(Theme::kMuted, "Select a name to see session coverage.");
        return;
    }

    const InstrumentId selected = selected_id_.value_or(0);
    std::string heading = "SESSIONS";
    for (const auto& row : summaries_)
    {
        if (row.instrument.id == selected)
        {
            heading = row.instrument.symbol;
            if (row.instrument.figi.has_value())
            {
                heading += "  " + *row.instrument.figi;
            }
            if (!row.instrument.listing_open)
            {
                heading += "  (no open listing)";
            }
            heading += "  " + std::to_string(days_.size()) + " sessions";
            if (!selected_history_.empty())
            {
                heading += "  " + selected_history_;
            }
            break;
        }
    }
    ImGui::TextUnformatted(heading.c_str());

    constexpr ImGuiTableFlags flags =
        ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;

    if (!ImGui::BeginTable("sessions", 6, flags, ImVec2(0.0f, 0.0f)))
    {
        return;
    }
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("DATE");
    ImGui::TableSetupColumn("STATUS", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("BARS", ImGuiTableColumnFlags_WidthFixed, 56.0f);
    ImGui::TableSetupColumn("Expected", ImGuiTableColumnFlags_WidthFixed, 72.0f);
    ImGui::TableSetupColumn("FIRST TS");
    ImGui::TableSetupColumn("INGESTED");
    ImGui::TableHeadersRow();

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(days_.size()));
    while (clipper.Step())
    {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
        {
            const CoverageDay& day = days_[static_cast<std::size_t>(i)];
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            const std::string session = formatSessionDate(day.session_date);
            cellMono(session.c_str());
            ImGui::TableNextColumn();
            const std::string status_text(toSql(day.status));
            ImGui::TextColored(statusColor(day.status), "%s", status_text.c_str());
            ImGui::TableNextColumn();
            cellInt(day.bar_count);
            ImGui::TableNextColumn();
            if (day.expected_count.has_value())
            {
                const int expected = day.expected_count.value_or(0);
                cellInt(expected);
            }
            ImGui::TableNextColumn();
            if (day.first_ts.has_value())
            {
                const std::string first = formatUtcMinute(day.first_ts.value_or(0));
                cellMono(first.c_str());
            }
            ImGui::TableNextColumn();
            const std::string ingested = formatUtcMinute(day.ingested_at);
            cellMono(ingested.c_str());
        }
    }
    ImGui::EndTable();
}

}  // namespace terminal
