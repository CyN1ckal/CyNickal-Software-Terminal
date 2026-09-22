// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "ui/InventoryPanel.h"

#include "RepoRoot.h"
#include "data/IngestWorker.h"
#include "ui/Theme.h"

#include "market_data/NyseCalendar.h"
#include "market_data/Time.h"

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
constexpr int kColExchange = 1;
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
    const int lookback = timeframe_s == kTimeframe1d ? 365 * 5 : 14;
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

void cellInt(int value)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d", value);
    cellRight(buf);
}

[[nodiscard]] std::string formatUtcMinute(UnixSeconds ts)
{
    const auto t = static_cast<std::time_t>(ts);
    std::tm utc{};
    if (gmtime_r(&t, &utc) == nullptr)
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
        worker_->enqueue(IngestWorker::Job{symbol_, from, to, ingest_timeframe_s_});
        status_ = std::string("queued ") + symbol_ + " " + formatSessionDate(from) + ".." +
                  formatSessionDate(to);
    }
    catch (const std::exception& ex)
    {
        status_ = ex.what();
    }
}

void InventoryPanel::draw()
{
    if (!ImGui::Begin("DATA"))
    {
        ImGui::End();
        return;
    }

    if (!open_error_.empty() && store_ == nullptr)
    {
        ImGui::TextColored(Theme::kDown, "%s", open_error_.c_str());
        ImGui::End();
        return;
    }

    pollWorker();
    drawToolbar();
    ImGui::Separator();
    ImGui::TextColored(Theme::kMuted, "%s", status_.c_str());

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
    ImGui::End();
}

void InventoryPanel::drawToolbar()
{
    ImGui::PushStyleColor(ImGuiCol_FrameBg, Theme::kField);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Theme::kBg3);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Theme::kBg3);

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("TF");
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
    ImGui::TextUnformatted("SYMBOL");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(72.0f);
    const bool symbol_go =
        ImGui::InputText("##symbol", symbol_, sizeof(symbol_),
                         ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    ImGui::TextUnformatted("FROM");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(88.0f);
    const bool from_go = ImGui::InputText("##from", from_, sizeof(from_),
                                          ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    ImGui::TextUnformatted("TO");
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
    std::sort(summaries_.begin(), summaries_.end(), [&](const CoverageSummary& a, const CoverageSummary& b) {
        int delta = 0;
        switch (col)
        {
        case kColSymbol:
            delta = a.instrument.symbol.compare(b.instrument.symbol);
            break;
        case kColExchange:
            delta = a.instrument.exchange.value_or("").compare(b.instrument.exchange.value_or(""));
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
    ImGui::TableSetupColumn("EXCH", ImGuiTableColumnFlags_WidthFixed, 48.0f);
    ImGui::TableSetupColumn("TF", ImGuiTableColumnFlags_WidthFixed, 36.0f);
    ImGui::TableSetupColumn("BARS", ImGuiTableColumnFlags_WidthFixed, 64.0f);
    ImGui::TableSetupColumn("SESS", ImGuiTableColumnFlags_WidthFixed, 48.0f);
    ImGui::TableSetupColumn("OK", ImGuiTableColumnFlags_WidthFixed, 40.0f);
    ImGui::TableSetupColumn("PART", ImGuiTableColumnFlags_WidthFixed, 44.0f);
    ImGui::TableSetupColumn("MISS", ImGuiTableColumnFlags_WidthFixed, 44.0f);
    ImGui::TableSetupColumn("ERR", ImGuiTableColumnFlags_WidthFixed, 40.0f);
    ImGui::TableSetupColumn("FIRST");
    ImGui::TableSetupColumn("LAST");
    ImGui::TableHeadersRow();
    applySortSpecs();

    if (summaries_.empty())
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(Theme::kMuted, "No names yet. Enter a symbol and GO.");
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
                ingest_timeframe_s_ = row.timeframe_s;
                std::snprintf(symbol_, sizeof(symbol_), "%s", row.instrument.symbol.c_str());
                refreshDays();
            }
            ImGui::TableNextColumn();
            const std::string exchange = row.instrument.exchange.value_or("");
            ImGui::TextUnformatted(exchange.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(timeframeLabel(row.timeframe_s));
            ImGui::TableNextColumn();
            cellInt(row.bar_count);
            ImGui::TableNextColumn();
            cellInt(row.session_count);
            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, Theme::kUp);
            cellInt(row.complete_count);
            ImGui::PopStyleColor();
            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  row.partial_count > 0 ? Theme::kWarn : Theme::kMuted);
            cellInt(row.partial_count);
            ImGui::PopStyleColor();
            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, Theme::kMuted);
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
            ImGui::TextUnformatted(first.c_str());
            ImGui::TableNextColumn();
            const SessionDate last_date = row.last_session.value_or(0);
            const std::string last =
                last_date != 0 ? formatSessionDate(last_date) : std::string{};
            ImGui::TextUnformatted(last.c_str());
        }
    }
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
            heading = row.instrument.symbol + "  " + std::to_string(days_.size()) + " sessions";
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
    ImGui::TableSetupColumn("EXP", ImGuiTableColumnFlags_WidthFixed, 48.0f);
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
            ImGui::TextUnformatted(formatSessionDate(day.session_date).c_str());
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
                ImGui::TextUnformatted(first.c_str());
            }
            ImGui::TableNextColumn();
            const std::string ingested = formatUtcMinute(day.ingested_at);
            ImGui::TextUnformatted(ingested.c_str());
        }
    }
    ImGui::EndTable();
}

}  // namespace terminal
