// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "ui/PortfolioPanel.h"

#include "IngestDefaults.h"
#include "data/PortfolioFetch.h"
#include "risk/HistoricalRisk.h"
#include "ui/Theme.h"

#include "market_data/Adjust.h"
#include "market_data/NyseCalendar.h"
#include "market_data/Store.h"
#include "market_data/Time.h"

#include "implot.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <limits>
#include <string>
#include <utility>

namespace terminal {
namespace {

// Listed equity options in this store are 100 shares. The holding does not store a multiplier.
constexpr double kOptionContractMultiplier = 100.0;
constexpr const char* kKinds[] = {"equity", "etf", "option", "cash"};
constexpr const char* kExpirationTypes[] = {"weekly", "monthly"};
constexpr const char* kRights[] = {"call", "put"};

[[nodiscard]] std::string trim(std::string_view text)
{
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
    {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t'))
    {
        text.remove_suffix(1);
    }
    return std::string(text);
}

[[nodiscard]] PortfolioAssetKind kindAt(int index)
{
    switch (index)
    {
    case 1:
        return PortfolioAssetKind::Etf;
    case 2:
        return PortfolioAssetKind::Option;
    case 3:
        return PortfolioAssetKind::Cash;
    default:
        return PortfolioAssetKind::Equity;
    }
}

[[nodiscard]] bool parseDouble(std::string_view text, double& out)
{
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const auto parsed = std::from_chars(begin, end, out);
    return parsed.ec == std::errc{} && parsed.ptr == end && std::isfinite(out);
}

// Commas and a leading dollar sign are display characters. A half-typed minus or dot is not a number yet.
[[nodiscard]] bool parseLooseDouble(std::string_view text, double& out)
{
    std::string compact;
    compact.reserve(text.size());
    for (const char ch : text)
    {
        if (ch == ',' || ch == ' ' || ch == '$')
        {
            continue;
        }
        compact.push_back(ch);
    }
    if (compact.empty() || compact == "-" || compact == "." || compact == "-.")
    {
        return false;
    }
    return parseDouble(compact, out);
}

[[nodiscard]] bool parseDate(std::string_view text, SessionDate& out)
{
    int value = 0;
    const char* const begin = text.data();
    const auto parsed = std::from_chars(begin, begin + text.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != begin + text.size() || text.size() != 8)
    {
        return false;
    }
    const int year = value / 10000;
    const auto month = static_cast<unsigned>((value / 100) % 100);
    const auto day = static_cast<unsigned>(value % 100);
    const std::chrono::year_month_day ymd{std::chrono::year{year}, std::chrono::month{month},
                                          std::chrono::day{day}};
    if (!ymd.ok())
    {
        return false;
    }
    out = static_cast<SessionDate>(value);
    return true;
}

[[nodiscard]] bool kindMatches(PortfolioAssetKind kind, AssetClass asset_class)
{
    switch (kind)
    {
    case PortfolioAssetKind::Equity:
        return asset_class == AssetClass::Equity;
    case PortfolioAssetKind::Etf:
        return asset_class == AssetClass::Etf;
    case PortfolioAssetKind::Option:
        return asset_class == AssetClass::Equity || asset_class == AssetClass::Etf ||
               asset_class == AssetClass::Index;
    case PortfolioAssetKind::Cash:
        return false;
    }
    return false;
}

[[nodiscard]] std::string symbolText(const PortfolioHolding& row)
{
    if (row.kind == PortfolioAssetKind::Cash)
    {
        return "USD";
    }
    if (!row.symbol.has_value())
    {
        return {};
    }
    if (!row.listing_open)
    {
        return *row.symbol + " closed";
    }
    return *row.symbol;
}

template <typename Fn>
bool withWriter(Store* reader, std::string& error, Fn&& action)
{
    if (reader == nullptr)
    {
        error = "market data is unavailable";
        return false;
    }
    try
    {
        Store writer(reader->path(), StoreMode::Writer);
        std::forward<Fn>(action)(writer);
        error.clear();
        return true;
    }
    catch (const std::exception& ex)
    {
        error = ex.what();
        return false;
    }
}

[[nodiscard]] std::optional<std::pair<UnixSeconds, double>> latestClose(const Store& store,
                                                                        InstrumentId id,
                                                                        int timeframe_s)
{
    const std::vector<CoverageDay> days = store.queryCoverageDays(id, timeframe_s);
    UnixSeconds latest = -1;
    for (const CoverageDay& day : days)
    {
        if (day.bar_count > 0 && day.last_ts.has_value() && *day.last_ts > latest)
        {
            latest = *day.last_ts;
        }
    }
    if (latest < 0)
    {
        return std::nullopt;
    }
    const std::vector<Bar> bars = store.queryBars(id, timeframe_s, latest, latest + 1);
    if (bars.empty())
    {
        return std::nullopt;
    }
    return std::pair<UnixSeconds, double>{bars.back().ts, bars.back().close};
}

[[nodiscard]] std::optional<double> equityLast(const Store& store, InstrumentId id)
{
    const std::optional<std::pair<UnixSeconds, double>> daily = latestClose(store, id, kTimeframe1d);
    const std::optional<std::pair<UnixSeconds, double>> minute = latestClose(store, id, kTimeframe1m);
    if (!daily.has_value())
    {
        return minute.has_value() ? std::optional<double>{minute->second} : std::nullopt;
    }
    if (!minute.has_value() || minute->first <= daily->first)
    {
        return daily->second;
    }
    return minute->second;
}

struct OptionSnapshot
{
    double last{0.0};
    double delta{0.0};
};

[[nodiscard]] std::optional<OptionSnapshot> optionSnapshot(const Store& store, const PortfolioHolding& row)
{
    if (!row.instrument_id.has_value() || !row.expiration.has_value() || !row.expiration_type.has_value() ||
        !row.strike.has_value() || !row.right.has_value())
    {
        return std::nullopt;
    }
    const std::vector<OptionQuote> quotes =
        store.queryOptionQuotes(*row.instrument_id, *row.expiration, *row.expiration_type);
    for (const OptionQuote& quote : quotes)
    {
        if (quote.right != *row.right || std::abs(quote.strike - *row.strike) > 0.0001)
        {
            continue;
        }
        if (row.vendor_symbol.has_value() && quote.vendor_symbol != *row.vendor_symbol)
        {
            continue;
        }
        OptionSnapshot snapshot;
        snapshot.last = quote.last;
        snapshot.delta = quote.delta;
        return snapshot;
    }
    return std::nullopt;
}

[[nodiscard]] std::vector<double> dailyCloses(const Store& store, InstrumentId id)
{
    constexpr UnixSeconds kAllBarsEnd = std::numeric_limits<UnixSeconds>::max();
    std::vector<Bar> bars = store.queryBars(id, kTimeframe1d, 0, kAllBarsEnd);
    if (!bars.empty())
    {
        // End at the last bar: a split on that bar is left unchanged, so the last close stays as-traded.
        const std::vector<CorporateAction> actions =
            store.queryCorporateActions(id, 0, bars.back().ts);
        bars = adjustBarsForSplits(std::move(bars), actions);
    }
    std::vector<double> closes;
    closes.reserve(bars.size());
    for (const Bar& bar : bars)
    {
        closes.push_back(bar.close);
    }
    return closes;
}

[[nodiscard]] double lineValue(const PortfolioHolding& row, double last)
{
    if (row.kind == PortfolioAssetKind::Option)
    {
        return row.quantity * last * kOptionContractMultiplier;
    }
    return row.quantity * last;
}

void appendGrouped(std::string& out, std::string_view whole)
{
    const int lead = static_cast<int>(whole.size() % 3);
    for (int index = 0; std::cmp_less(index, whole.size()); ++index)
    {
        if (index > 0 && (index - (lead == 0 ? 3 : lead)) % 3 == 0)
        {
            out.push_back(',');
        }
        out.push_back(whole[static_cast<std::size_t>(index)]);
    }
}

// Fixed-point text. Money prefixes '$'. trim drops trailing fractional zeros (100.0000 -> 100).
[[nodiscard]] std::string formatFixed(double amount, int decimals, bool money, bool trim)
{
    if (!std::isfinite(amount) || decimals < 0 || decimals > 8)
    {
        return {};
    }
    char raw[96];
    const int written = std::snprintf(raw, sizeof(raw), "%.*f", decimals, std::fabs(amount));
    if (written <= 0 || static_cast<std::size_t>(written) >= sizeof(raw))
    {
        return {};
    }
    const std::string_view text(raw, static_cast<std::size_t>(written));
    const bool zero = std::ranges::all_of(text, [](char ch) { return ch == '0' || ch == '.'; });
    const bool negative = amount < 0.0 && !zero;
    const std::size_t dot = text.find('.');
    std::string_view whole = dot == std::string_view::npos ? text : text.substr(0, dot);
    std::string fraction;
    if (dot != std::string_view::npos)
    {
        fraction.assign(text.substr(dot));
        if (trim)
        {
            while (!fraction.empty() && fraction.back() == '0')
            {
                fraction.pop_back();
            }
            if (fraction == ".")
            {
                fraction.clear();
            }
        }
    }
    if (whole.empty())
    {
        whole = "0";
    }
    std::string out;
    out.reserve(text.size() + 8);
    if (negative)
    {
        out.push_back('-');
    }
    if (money)
    {
        out.push_back('$');
    }
    appendGrouped(out, whole);
    out += fraction;
    return out;
}

[[nodiscard]] std::string formatUsd(double amount, int decimals)
{
    return formatFixed(amount, decimals, true, false);
}

[[nodiscard]] std::string formatQuantity(double quantity)
{
    return formatFixed(quantity, 4, false, true);
}

// Quotes under a dollar keep the extra places option premiums need. Larger prints are cents.
[[nodiscard]] int priceDecimals(double amount)
{
    const double magnitude = std::fabs(amount);
    if (magnitude > 0.0 && magnitude < 1.0)
    {
        return 4;
    }
    return 2;
}

void drawRight(const char* text, const ImVec4& color)
{
    const float width = ImGui::GetContentRegionAvail().x;
    const float text_w = ImGui::CalcTextSize(text).x;
    if (text_w < width)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + width - text_w);
    }
    ImGui::TextColored(color, "%s", text);
}

void drawMoney(double amount, int decimals)
{
    const std::string text = formatUsd(amount, decimals);
    ImFont* const mono = Theme::monoFont();
    if (mono != nullptr)
    {
        ImGui::PushFont(mono);
    }
    drawRight(text.c_str(), amount < 0.0 ? Theme::kDown : Theme::kText);
    if (mono != nullptr)
    {
        ImGui::PopFont();
    }
}

[[nodiscard]] std::string positionLabel(const PortfolioHolding& row)
{
    std::string label = symbolText(row);
    if (label.empty())
    {
        const std::string_view kind = toSql(row.kind);
        label.assign(kind.data(), kind.size());
    }
    if (row.kind == PortfolioAssetKind::Option)
    {
        const std::optional<SessionDate> expiration = row.expiration;
        if (expiration.has_value())
        {
            label.push_back(' ');
            label += formatSessionDate(expiration.value());
        }
        const std::optional<double> strike = row.strike;
        if (strike.has_value())
        {
            label.push_back(' ');
            label += formatUsd(strike.value(), priceDecimals(strike.value()));
        }
        const std::optional<OptionRight> side = row.right;
        if (side.has_value())
        {
            label.push_back(' ');
            const std::string_view name = toSql(side.value());
            label.append(name.data(), name.size());
        }
    }
    if (row.quantity < 0.0)
    {
        label += " short";
    }
    return label;
}

[[nodiscard]] std::string uniqueLabel(std::string_view label, const std::vector<std::string>& used)
{
    const auto same = [&](const std::string& existing) { return existing == label; };
    if (std::ranges::find_if(used, same) == used.end())
    {
        return std::string(label);
    }
    for (int suffix = 2; suffix < 10000; ++suffix)
    {
        const std::string candidate = std::string(label) + " " + std::to_string(suffix);
        if (std::ranges::find_if(used, [&](const std::string& existing) { return existing == candidate; }) == used.end())
        {
            return candidate;
        }
    }
    return std::string(label);
}

struct SliceLabel
{
    double sum = 0.0;
};

struct HoldingsColumns
{
    int unit_var{-1};
    int position_cvar{-1};
    int unit_cvar{-1};
    int portfolio_var{-1};
    int expiration{0};
    int expiry_type{0};
    int strike{0};
    int right{0};
    int count{0};
};

void setupHoldingsColumns(HoldingsColumns& columns, bool position, bool unit)
{
    constexpr ImGuiTableColumnFlags kFixed = ImGuiTableColumnFlags_WidthFixed;
    int next = 0;
    const auto add = [&](const char* name, ImGuiTableColumnFlags flags, float width) {
        ImGui::TableSetupColumn(name, flags, width);
        const int index = next;
        ++next;
        return index;
    };
    add("Kind", kFixed, 72.f);
    add("Symbol", ImGuiTableColumnFlags_WidthStretch, 1.f);
    add("FIGI", kFixed, 136.f);
    add("Quantity", kFixed, 108.f);
    add("Last", kFixed, 128.f);
    add("Value", kFixed, 148.f);
    if (position)
    {
        columns.position_cvar = add("Position CVaR", kFixed, 168.f);
    }
    if (unit)
    {
        columns.unit_var = add("Unit VaR", kFixed, 148.f);
        columns.unit_cvar = add("Unit CVaR", kFixed, 156.f);
    }
    columns.portfolio_var = add("Portfolio VaR", kFixed, 168.f);
    columns.expiration = add("Expiration", kFixed, 108.f);
    columns.expiry_type = add("Type", kFixed, 84.f);
    columns.strike = add("Strike", kFixed, 120.f);
    columns.right = add("Right", kFixed, 136.f);
    columns.count = next;
}

void drawHoldingsHeaders(const HoldingsColumns& columns, int confidence_pct)
{
    const std::string pct = std::to_string(confidence_pct) + "%";
    const std::string position_cvar = "CVaR " + pct;
    const std::string unit_var = "Unit VaR " + pct;
    const std::string unit_cvar = "Unit CVaR " + pct;
    const std::string unit_var_tip =
        "One share, or one option contract, in the same direction as the line. "
        "A flat quantity shows one long unit. Same " +
        pct + " history as the position figure.";
    const std::string position_cvar_tip =
        "Position 1-day " + pct + " expected shortfall: the average P&L beyond VaR. A loss prints negative.";
    const std::string unit_cvar_tip = "Per-unit 1-day " + pct +
                                      " expected shortfall. A loss prints negative.";
    const std::string portfolio_var = "Portfolio " + pct;
    const std::string portfolio_tip =
        "Sum of each line's own 1-day " + pct +
        " historical VaR, from that line's latest daily returns. A loss prints negative.";

    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
    const int count = ImGui::TableGetColumnCount();
    for (int column = 0; column < count; ++column)
    {
        ImGui::TableSetColumnIndex(column);
        const char* name = ImGui::TableGetColumnName(column);
        const char* tip = nullptr;
        if (column == columns.unit_var)
        {
            name = unit_var.c_str();
            tip = unit_var_tip.c_str();
        }
        else if (column == columns.position_cvar)
        {
            name = position_cvar.c_str();
            tip = position_cvar_tip.c_str();
        }
        else if (column == columns.unit_cvar)
        {
            name = unit_cvar.c_str();
            tip = unit_cvar_tip.c_str();
        }
        else if (column == columns.portfolio_var)
        {
            name = portfolio_var.c_str();
            tip = portfolio_tip.c_str();
        }
        ImGui::PushID(column);
        ImGui::TableHeader(name);
        if (tip != nullptr && ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", tip);
        }
        ImGui::PopID();
    }
}

void drawRiskPnl(double loss, bool per_unit)
{
    const double pnl = -loss;
    drawMoney(pnl, per_unit ? priceDecimals(pnl) : 2);
}

int formatSliceShare(double value, char* buffer, int size, void* data)  // NOLINT(misc-const-correctness)
{
    if (buffer == nullptr || size <= 1)
    {
        return 0;
    }
    buffer[0] = '\0';
    const auto* label = static_cast<const SliceLabel*>(data);
    if (label == nullptr || !(label->sum > 0.0))
    {
        return 0;
    }
    const double share = value / label->sum * 100.0;
    // Small wedges cannot hold a readable percent. The legend still names them.
    if (share < 7.0)
    {
        return 0;
    }
    return std::snprintf(buffer, static_cast<std::size_t>(size), "%.0f%%", share);
}

void enqueueSymbol(IngestWorker& ingest, std::string symbol, std::uint64_t& serial)
{
    const SessionDate today = utcToSessionDate("America/New_York", nowUtc());
    const auto ymd = sessionDateToYmd(today);
    IngestWorker::Job job;
    job.symbol = std::move(symbol);
    job.from = toSessionDate(std::chrono::sys_days{ymd} - std::chrono::days{kIngestDefaultDailyDays});
    job.to = today;
    job.timeframe_s = kTimeframe1d;
    serial = ingest.enqueue(std::move(job)).serial;
}

}  // namespace

PortfolioPanel::PortfolioPanel(int id) : id_(id) {}

int PortfolioPanel::id() const noexcept
{
    return id_;
}

bool PortfolioPanel::windowOpen() const noexcept
{
    return window_open_;
}

void PortfolioPanel::closeWindow()
{
    window_open_ = false;
}

void PortfolioPanel::requestFocus()
{
    focus_on_appear_ = true;
}

void PortfolioPanel::importState(const ChartbookPortfolio& state)
{
    portfolio_id_ = state.portfolio_id;
    show_position_var_ = state.show_position_var;
    show_unit_var_ = state.show_unit_var;
    var_confidence_pct_ = state.var_confidence_pct >= 50 && state.var_confidence_pct <= 99 ? state.var_confidence_pct
                                                                                            : 95;
    loaded_ = false;
    dirty_ = false;
}

ChartbookPortfolio PortfolioPanel::exportState() const
{
    ChartbookPortfolio state;
    state.id = id_;
    state.portfolio_id = portfolio_id_;
    state.var_confidence_pct = var_confidence_pct_;
    state.show_position_var = show_position_var_;
    state.show_unit_var = show_unit_var_;
    return state;
}

void PortfolioPanel::setWindowScope(int runtime_id) noexcept
{
    runtime_id_ = runtime_id;
}

void PortfolioPanel::setPlacement(bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size)
{
    place_force_ = force;
    place_floating_ = floating;
    place_dock_ = dock;
    place_pos_ = pos;
    place_size_ = size;
}

void PortfolioPanel::reload(Store& store)
{
    books_ = store.listPortfolios();
    const auto found = std::ranges::find_if(books_, [&](const Portfolio& book) {
        return book.id == portfolio_id_;
    });
    if (portfolio_id_ != 0 && found == books_.end())
    {
        portfolio_id_ = 0;
        drafts_.clear();
        quantity_edit_ = -1;
        book_name_.clear();
        loaded_ = false;
        dirty_ = false;
        marks_valid_ = false;
        status_ = "portfolio was deleted";
        return;
    }
    if (portfolio_id_ == 0)
    {
        drafts_.clear();
        quantity_edit_ = -1;
        book_name_.clear();
        loaded_ = true;
        loaded_updated_ = 0;
        marks_valid_ = false;
        return;
    }
    if (!dirty_ && (!loaded_ || found->updated_at != loaded_updated_))
    {
        drafts_ = store.queryHoldings(portfolio_id_);
        quantity_edit_ = -1;
        marks_valid_ = false;
        book_name_ = found->name;
        loaded_updated_ = found->updated_at;
        loaded_ = true;
        const std::size_t count = std::min(book_name_.size(), sizeof(rename_) - 1);
        std::copy_n(book_name_.data(), count, rename_);
        rename_[count] = '\0';
        status_ = book_name_;
    }
}

bool PortfolioPanel::appendResolved(const Store& store, PortfolioAssetKind kind, const std::string& symbol,
                                    double quantity)
{
    const std::optional<Instrument> instrument = store.findOpenListing(symbol);
    if (!instrument.has_value() || !instrument->figi.has_value())
    {
        return false;
    }
    if (!kindMatches(kind, instrument->asset_class))
    {
        error_ = symbol + " is " + std::string(toSql(instrument->asset_class));
        status_ = error_;
        return true;
    }
    PortfolioHolding row;
    row.kind = kind;
    row.figi = instrument->figi;
    row.instrument_id = instrument->id;
    row.symbol = instrument->symbol;
    row.listing_open = true;
    row.quantity = quantity;
    if (kind == PortfolioAssetKind::Option)
    {
        row.expiration = pending_expiration_;
        row.expiration_type = pending_expiration_type_;
        row.strike = pending_strike_;
        row.right = pending_right_;
    }
    drafts_.push_back(std::move(row));
    dirty_ = true;
    marks_valid_ = false;
    error_.clear();
    status_ = "unsaved";
    return true;
}

void PortfolioPanel::pollPending(Store& store, IngestWorker* ingest)
{
    if (!pending_ || ingest == nullptr)
    {
        return;
    }
    const IngestWorker::Snapshot snap = ingest->snapshot();
    if (snap.finished_serial < pending_serial_ || pending_serial_ == 0)
    {
        return;
    }
    const IngestWorker::SerialFailure failure = ingest->failureForSerial(pending_serial_);
    pending_ = false;
    if (failure.failed)
    {
        error_ = failure.message;
        status_ = failure.message;
        return;
    }
    if (!appendResolved(store, pending_kind_, pending_symbol_, pending_quantity_))
    {
        error_ = pending_symbol_ + " has no open listing";
        status_ = error_;
    }
}

void PortfolioPanel::createBook(Store& store)
{
    const std::string name = trim(new_name_);
    bool created = false;
    PortfolioId id = 0;
    if (!withWriter(&store, error_, [&](Store& writer) {
            id = writer.createPortfolio(name);
            created = true;
        }))
    {
        status_ = error_;
        return;
    }
    if (!created)
    {
        return;
    }
    portfolio_id_ = id;
    loaded_ = false;
    dirty_ = false;
    drafts_.clear();
    quantity_edit_ = -1;
    new_name_[0] = '\0';
    book_name_ = name;
    status_ = name;
}

void PortfolioPanel::renameBook(Store& store)
{
    if (portfolio_id_ == 0)
    {
        return;
    }
    const std::string name = trim(rename_);
    if (!withWriter(&store, error_, [&](Store& writer) { writer.renamePortfolio(portfolio_id_, name); }))
    {
        status_ = error_;
        return;
    }
    book_name_ = name;
    loaded_ = false;
    status_ = name;
}

void PortfolioPanel::deleteBook(Store& store)
{
    if (portfolio_id_ == 0)
    {
        return;
    }
    if (!withWriter(&store, error_, [&](Store& writer) { writer.deletePortfolio(portfolio_id_); }))
    {
        status_ = error_;
        return;
    }
    portfolio_id_ = 0;
    drafts_.clear();
    quantity_edit_ = -1;
    book_name_.clear();
    loaded_ = false;
    dirty_ = false;
    marks_valid_ = false;
    status_ = "select a portfolio";
}

void PortfolioPanel::apply(Store& store, IngestWorker* ingest)
{
    if (portfolio_id_ == 0)
    {
        return;
    }
    if (!withWriter(&store, error_, [&](Store& writer) {
            writer.replaceHoldings(portfolio_id_, drafts_);
            drafts_ = writer.queryHoldings(portfolio_id_);
            quantity_edit_ = -1;
            marks_valid_ = false;
            if (ingest != nullptr)
            {
                const SessionDate today = utcToSessionDate("America/New_York", nowUtc());
                enqueuePortfolioFetches(*ingest, portfolioFetchJobs(writer, portfolio_id_, today));
            }
        }))
    {
        status_ = error_;
        return;
    }
    dirty_ = false;
    loaded_ = false;
    status_ = "saved";
}

void PortfolioPanel::drawBooks(Store& store)
{
    const char* preview = book_name_.empty() ? "select a portfolio" : book_name_.c_str();
    ImGui::SetNextItemWidth(180.f);
    if (ImGui::BeginCombo("##book", preview))
    {
        for (const Portfolio& book : books_)
        {
            const bool selected = book.id == portfolio_id_;
            if (ImGui::Selectable(book.name.c_str(), selected) && !dirty_ && book.id != portfolio_id_)
            {
                portfolio_id_ = book.id;
                loaded_ = false;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.f);
    ImGui::InputTextWithHint("##new", "new name", new_name_, sizeof(new_name_));
    ImGui::SameLine();
    if (ImGui::Button("New"))
    {
        createBook(store);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.f);
    ImGui::InputText("##rename", rename_, sizeof(rename_));
    ImGui::SameLine();
    if (ImGui::Button("Rename") && portfolio_id_ != 0)
    {
        renameBook(store);
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete") && portfolio_id_ != 0)
    {
        deleteBook(store);
    }
}

void PortfolioPanel::refreshMarks(const Store& store)
{
    lasts_.assign(drafts_.size(), std::nullopt);
    risks_.assign(drafts_.size(), HoldingRisk{});
    for (std::size_t index = 0; index < drafts_.size(); ++index)
    {
        const PortfolioHolding& row = drafts_[index];
        if (row.kind == PortfolioAssetKind::Cash)
        {
            lasts_[index] = 1.0;
            risks_[index].basis = HoldingRiskBasis::Cash;
            continue;
        }
        const std::optional<InstrumentId> instrument_id = row.instrument_id;
        if (!instrument_id.has_value())
        {
            continue;
        }
        const InstrumentId instrument = instrument_id.value();
        if (row.kind == PortfolioAssetKind::Option)
        {
            const std::optional<OptionSnapshot> found = optionSnapshot(store, row);
            if (!found.has_value())
            {
                continue;
            }
            const OptionSnapshot quote = found.value();
            lasts_[index] = quote.last;
            const std::optional<double> underlying = equityLast(store, instrument);
            if (!underlying.has_value())
            {
                continue;
            }
            const double spot = underlying.value();
            if (!(spot > 0.0) || !std::isfinite(quote.delta))
            {
                continue;
            }
            risks_[index].basis = HoldingRiskBasis::Delta;
            risks_[index].unit_exposure = optionDeltaExposure(1.0, kOptionContractMultiplier, quote.delta, spot);
            risks_[index].closes = dailyCloses(store, instrument);
            continue;
        }
        const std::optional<double> last = equityLast(store, instrument);
        if (!last.has_value())
        {
            continue;
        }
        const double price = last.value();
        lasts_[index] = price;
        if (!(price > 0.0))
        {
            continue;
        }
        risks_[index].basis = HoldingRiskBasis::Close;
        risks_[index].unit_exposure = price;
        risks_[index].closes = dailyCloses(store, instrument);
    }
    marks_valid_ = true;
}

void PortfolioPanel::drawHoldings(const Store& store)
{
    if (!marks_valid_ || lasts_.size() != drafts_.size() || risks_.size() != drafts_.size())
    {
        refreshMarks(store);
    }
    ImGui::Checkbox("CVaR", &show_position_var_);
    ImGui::SameLine();
    ImGui::Checkbox("Per unit", &show_unit_var_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(168.f);
    ImGui::SliderInt("##var_confidence", &var_confidence_pct_, 50, 99, "Confidence %d%%");

    const int risk_columns = (show_position_var_ ? 1 : 0) + (show_unit_var_ ? 2 : 0) + 1;
    const int column_count = 10 + risk_columns;
    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY |
                                  ImGuiTableFlags_ScrollX | ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings;
    const float inner_width =
        std::max(1160.f + (160.f * static_cast<float>(risk_columns)), ImGui::GetContentRegionAvail().x);
    if (!ImGui::BeginTable("holdings", column_count, flags, ImVec2(0.f, 0.f), inner_width))
    {
        return;
    }
    HoldingsColumns columns;
    setupHoldingsColumns(columns, show_position_var_, show_unit_var_);
    ImGui::TableSetupScrollFreeze(0, 1);
    drawHoldingsHeaders(columns, var_confidence_pct_);

    struct BookColumn
    {
        std::optional<double> total_loss;
        std::vector<std::optional<double>> row_loss;
    };
    const auto bookColumn = [&] {
        BookColumn column;
        column.row_loss.assign(drafts_.size(), std::nullopt);
        if (risks_.size() != drafts_.size())
        {
            return column;
        }
        std::vector<PortfolioLeg> legs;
        std::vector<std::size_t> rows;
        legs.reserve(drafts_.size());
        rows.reserve(drafts_.size());
        for (std::size_t index = 0; index < drafts_.size(); ++index)
        {
            const HoldingRisk& risk = risks_[index];
            if (risk.basis == HoldingRiskBasis::Unavailable)
            {
                continue;
            }
            const double quantity = drafts_[index].quantity;
            const double exposure = quantity * risk.unit_exposure;
            if (!std::isfinite(quantity) || !std::isfinite(exposure))
            {
                continue;
            }
            PortfolioLeg leg;
            leg.closes = risk.closes;
            leg.signed_exposure = exposure;
            legs.push_back(leg);
            rows.push_back(index);
        }
        ValueAtRiskSpec spec;
        spec.confidence = static_cast<double>(var_confidence_pct_) / 100.0;
        const PortfolioValueAtRisk measured = portfolioValueAtRisk(legs, spec);
        if (!measured.var.has_value() || measured.component_var.size() != legs.size())
        {
            return column;
        }
        column.total_loss = measured.var;
        for (std::size_t index = 0; index < rows.size(); ++index)
        {
            column.row_loss[rows[index]] = measured.component_var[index];
        }
        return column;
    };
    BookColumn book = bookColumn();

    for (std::size_t index = 0; index < drafts_.size(); ++index)
    {
        PortfolioHolding& row = drafts_[index];
        ImGui::TableNextRow();
        ImGui::PushID(static_cast<int>(index));
        ImGui::TableSetColumnIndex(0);
        const std::string_view kind = toSql(row.kind);
        ImGui::TextUnformatted(kind.data(), kind.data() + kind.size());
        ImGui::TableSetColumnIndex(1);
        const std::string symbol = symbolText(row);
        ImGui::TextUnformatted(symbol.c_str());
        ImGui::TableSetColumnIndex(2);
        if (row.figi.has_value())
        {
            ImGui::TextUnformatted(row.figi->c_str());
        }
        ImGui::TableSetColumnIndex(3);
        ImGui::SetNextItemWidth(-1.f);
        const int row_index = static_cast<int>(index);
        char quantity_buf[96];
        if (quantity_edit_ == row_index)
        {
            std::snprintf(quantity_buf, sizeof(quantity_buf), "%s", quantity_edit_buf_);
        }
        else
        {
            const std::string shown = formatQuantity(row.quantity);
            std::snprintf(quantity_buf, sizeof(quantity_buf), "%s", shown.c_str());
        }
        ImFont* const mono = Theme::monoFont();
        if (mono != nullptr)
        {
            ImGui::PushFont(mono);
        }
        const bool quantity_changed = ImGui::InputText("##qty", quantity_buf, sizeof(quantity_buf));
        if (mono != nullptr)
        {
            ImGui::PopFont();
        }
        if (ImGui::IsItemActive())
        {
            std::snprintf(quantity_edit_buf_, sizeof(quantity_edit_buf_), "%s", quantity_buf);
            quantity_edit_ = row_index;
        }
        else if (quantity_edit_ == row_index)
        {
            quantity_edit_ = -1;
        }
        if (quantity_changed)
        {
            double parsed = 0.0;
            if (parseLooseDouble(quantity_buf, parsed) && parsed != row.quantity)
            {
                row.quantity = parsed;
                dirty_ = true;
                status_ = "unsaved";
            }
        }
        const std::optional<double> last = index < lasts_.size() ? lasts_[index] : std::nullopt;
        ImGui::TableSetColumnIndex(4);
        if (last.has_value())
        {
            drawMoney(last.value(), priceDecimals(last.value()));
        }
        ImGui::TableSetColumnIndex(5);
        if (last.has_value())
        {
            drawMoney(lineValue(row, last.value()), 2);
        }
        HoldingValueAtRisk figures;
        if (index < risks_.size() && risks_[index].basis != HoldingRiskBasis::Unavailable)
        {
            ValueAtRiskSpec spec;
            spec.confidence = static_cast<double>(var_confidence_pct_) / 100.0;
            figures = holdingValueAtRisk(risks_[index].closes, row.quantity, risks_[index].unit_exposure, spec);
        }
        const auto draw_scope = [](int column, const std::optional<ValueAtRisk>& risk, bool cvar, bool per_unit) {
            if (column < 0)
            {
                return;
            }
            ImGui::TableSetColumnIndex(column);
            if (!risk.has_value())
            {
                return;
            }
            const ValueAtRisk measured = risk.value();
            drawRiskPnl(cvar ? measured.cvar : measured.var, per_unit);
        };
        draw_scope(columns.position_cvar, figures.position, true, false);
        draw_scope(columns.unit_var, figures.per_unit, false, true);
        draw_scope(columns.unit_cvar, figures.per_unit, true, true);
        ImGui::TableSetColumnIndex(columns.portfolio_var);
        if (index < book.row_loss.size())
        {
            const std::optional<double> loss = book.row_loss[index];
            if (loss.has_value())
            {
                drawRiskPnl(loss.value(), false);
            }
        }
        ImGui::TableSetColumnIndex(columns.expiration);
        if (row.expiration.has_value())
        {
            const std::string when = formatSessionDate(*row.expiration);
            ImGui::TextUnformatted(when.c_str());
        }
        ImGui::TableSetColumnIndex(columns.expiry_type);
        if (row.expiration_type.has_value())
        {
            const std::string_view type = toSql(*row.expiration_type);
            ImGui::TextUnformatted(type.data(), type.data() + type.size());
        }
        ImGui::TableSetColumnIndex(columns.strike);
        if (row.strike.has_value())
        {
            const std::optional<double> strike = row.strike;
            drawMoney(strike.value(), priceDecimals(strike.value()));
        }
        ImGui::TableSetColumnIndex(columns.right);
        if (row.right.has_value())
        {
            const std::string_view side = toSql(*row.right);
            ImGui::TextUnformatted(side.data(), side.data() + side.size());
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove"))
        {
            drafts_.erase(drafts_.begin() + static_cast<std::ptrdiff_t>(index));
            quantity_edit_ = -1;
            dirty_ = true;
            marks_valid_ = false;
            status_ = "unsaved";
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
    if (!marks_valid_ || lasts_.size() != drafts_.size() || risks_.size() != drafts_.size())
    {
        refreshMarks(store);
        book = bookColumn();
    }
    double total = 0.0;
    int unpriced = 0;
    for (std::size_t index = 0; index < drafts_.size(); ++index)
    {
        const std::optional<double> last = lasts_[index];
        if (!last.has_value())
        {
            ++unpriced;
            continue;
        }
        total += lineValue(drafts_[index], last.value());
    }
    ImGui::TableNextRow();
    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(Theme::kAccentWash));
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted("Total");
    ImGui::TableSetColumnIndex(4);
    if (unpriced > 0)
    {
        ImGui::TextColored(Theme::kWarn, "%d unpriced", unpriced);
    }
    ImGui::TableSetColumnIndex(5);
    drawMoney(total, 2);
    ImGui::TableSetColumnIndex(columns.portfolio_var);
    if (book.total_loss.has_value())
    {
        const double loss = book.total_loss.value();
        drawRiskPnl(loss, false);
    }
    ImGui::EndTable();
}

void PortfolioPanel::drawAllocation() const
{
    double gross = 0.0;
    int unpriced = 0;
    int shorts = 0;
    std::vector<std::string> labels;
    std::vector<double> weights;
    labels.reserve(drafts_.size());
    weights.reserve(drafts_.size());
    for (std::size_t index = 0; index < drafts_.size(); ++index)
    {
        const std::optional<double> last = index < lasts_.size() ? lasts_[index] : std::nullopt;
        if (!last.has_value())
        {
            ++unpriced;
            continue;
        }
        const double value = lineValue(drafts_[index], last.value());
        const double size = std::fabs(value);
        // A flat line has no wedge. Shorts still take a slice of their absolute size.
        if (!(size > 0.0))
        {
            continue;
        }
        if (drafts_[index].quantity < 0.0)
        {
            ++shorts;
        }
        gross += size;
        labels.push_back(uniqueLabel(positionLabel(drafts_[index]), labels));
        weights.push_back(size);
    }

    if (labels.empty())
    {
        ImGui::TextColored(Theme::kMuted, "no priced positions");
        if (unpriced > 0)
        {
            ImGui::TextColored(Theme::kWarn, "%d unpriced", unpriced);
        }
        return;
    }

    ImGui::TextColored(Theme::kMuted, "Positions");
    ImGui::SameLine();
    drawMoney(gross, 2);
    if (shorts > 0)
    {
        ImGui::TextColored(Theme::kMuted, "short positions use absolute value");
    }
    if (unpriced > 0)
    {
        ImGui::TextColored(Theme::kWarn, "%d unpriced", unpriced);
    }

    std::vector<const char*> label_ptrs;
    label_ptrs.reserve(labels.size());
    for (const std::string& label : labels)
    {
        label_ptrs.push_back(label.c_str());
    }
    SliceLabel slice;
    slice.sum = gross;
    constexpr ImPlotFlags plot_flags = ImPlotFlags_Equal | ImPlotFlags_NoMouseText | ImPlotFlags_NoMenus |
                                       ImPlotFlags_NoBoxSelect | ImPlotFlags_NoInputs;
    constexpr ImPlotAxisFlags axis_flags =
        ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoHighlight;
    ImPlotSpec spec;
    spec.Flags = ImPlotPieChartFlags_Normalize | ImPlotPieChartFlags_Exploding;
    ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(6.f, 6.f));
    ImPlot::PushStyleVar(ImPlotStyleVar_LegendPadding, ImVec2(8.f, 8.f));
    ImFont* const mono = Theme::monoFont();
    if (mono != nullptr)
    {
        ImGui::PushFont(mono);
    }
    if (ImPlot::BeginPlot("##positions", ImVec2(-1.f, -1.f), plot_flags))
    {
        ImPlot::SetupAxes(nullptr, nullptr, axis_flags, axis_flags);
        ImPlot::SetupAxesLimits(0.0, 1.0, 0.0, 1.0, ImPlotCond_Always);
        ImPlot::SetupLegend(ImPlotLocation_East, ImPlotLegendFlags_Outside | ImPlotLegendFlags_NoMenus);
        const auto count = static_cast<int>(weights.size());
        ImPlot::PlotPieChart(label_ptrs.data(), weights.data(), count, 0.5, 0.5, 0.4, formatSliceShare, &slice, 90.0,
                             spec);
        for (std::size_t index = 0; index < label_ptrs.size(); ++index)
        {
            if (!ImPlot::IsLegendEntryHovered(label_ptrs[index]))
            {
                continue;
            }
            const std::string amount = formatUsd(weights[index], 2);
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(amount.c_str());
            ImGui::EndTooltip();
            break;
        }
        ImPlot::EndPlot();
    }
    if (mono != nullptr)
    {
        ImGui::PopFont();
    }
    ImPlot::PopStyleVar(2);
}

void PortfolioPanel::drawAdd(Store& store, IngestWorker* ingest)
{
    if (portfolio_id_ == 0)
    {
        return;
    }
    ImGui::SetNextItemWidth(90.f);
    ImGui::Combo("##kind", &kind_, kKinds, 4);
    const bool cash = kindAt(kind_) == PortfolioAssetKind::Cash;
    if (!cash)
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.f);
        ImGui::InputTextWithHint("##symbol", "symbol", symbol_, sizeof(symbol_));
    }
    if (kindAt(kind_) == PortfolioAssetKind::Option)
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.f);
        ImGui::InputTextWithHint("##expiration", "YYYYMMDD", expiration_, sizeof(expiration_));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.f);
        ImGui::Combo("##expiry_type", &expiration_type_, kExpirationTypes, 2);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.f);
        ImGui::InputTextWithHint("##strike", "strike", strike_, sizeof(strike_));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.f);
        ImGui::Combo("##right", &right_, kRights, 2);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.f);
    ImGui::InputTextWithHint("##add_qty", "quantity", quantity_, sizeof(quantity_));
    ImGui::SameLine();
    if (!ImGui::Button("Add") || pending_)
    {
        return;
    }
    double quantity = 0.0;
    if (!parseDouble(trim(quantity_), quantity) || quantity == 0.0)
    {
        error_ = "quantity must be a non-zero number";
        status_ = error_;
        return;
    }
    const PortfolioAssetKind kind = kindAt(kind_);
    if (kind == PortfolioAssetKind::Cash)
    {
        PortfolioHolding row;
        row.kind = PortfolioAssetKind::Cash;
        row.quantity = quantity;
        drafts_.push_back(std::move(row));
        dirty_ = true;
        marks_valid_ = false;
        status_ = "unsaved";
        error_.clear();
        return;
    }
    const std::string symbol = trim(symbol_);
    if (symbol.empty())
    {
        error_ = "symbol is empty";
        status_ = error_;
        return;
    }
    SessionDate expiration = 0;
    double strike = 0.0;
    if (kind == PortfolioAssetKind::Option)
    {
        if (!parseDate(trim(expiration_), expiration))
        {
            error_ = "expiration must be YYYYMMDD";
            status_ = error_;
            return;
        }
        if (!parseDouble(trim(strike_), strike) || strike <= 0.0)
        {
            error_ = "strike must be positive";
            status_ = error_;
            return;
        }
        pending_expiration_ = expiration;
        pending_expiration_type_ =
            expiration_type_ == 0 ? OptionExpirationType::Weekly : OptionExpirationType::Monthly;
        pending_strike_ = strike;
        pending_right_ = right_ == 0 ? OptionRight::Call : OptionRight::Put;
    }
    if (appendResolved(store, kind, symbol, quantity))
    {
        return;
    }
    if (ingest == nullptr)
    {
        error_ = symbol + " has no open listing";
        status_ = error_;
        return;
    }
    pending_ = true;
    pending_kind_ = kind;
    pending_symbol_ = symbol;
    pending_quantity_ = quantity;
    enqueueSymbol(*ingest, symbol, pending_serial_);
    error_.clear();
    status_ = "fetching " + symbol;
}

bool PortfolioPanel::draw(Store* store, std::string_view store_error, IngestWorker* ingest)
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

    const std::string title = (book_name_.empty() ? std::string("PORTFOLIO") : book_name_) + "###cb" +
                              std::to_string(runtime_id_) + "_portfolio" + std::to_string(id_);
    if (!ImGui::Begin(title.c_str(), &window_open_, ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::End();
        return false;
    }
    if (store == nullptr)
    {
        if (!store_error.empty())
        {
            ImGui::TextColored(Theme::kDown, "%s", std::string(store_error).c_str());
        }
        ImGui::End();
        return ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    }

    reload(*store);
    pollPending(*store, ingest);
    if (ingest != nullptr && ingest->snapshot().finished_serial != priced_serial_)
    {
        priced_serial_ = ingest->snapshot().finished_serial;
        marks_valid_ = false;
    }
    drawBooks(*store);
    if (dirty_)
    {
        ImGui::SameLine();
        if (ImGui::Button("Apply"))
        {
            apply(*store, ingest);
        }
        ImGui::SameLine();
        if (ImGui::Button("Revert"))
        {
            dirty_ = false;
            loaded_ = false;
        }
    }
    ImGui::Separator();
    ImVec4 status_color = Theme::kMuted;
    if (pending_)
    {
        status_color = Theme::kAccent;
    }
    else if (!error_.empty())
    {
        status_color = Theme::kDown;
    }
    else if (dirty_)
    {
        status_color = Theme::kWarn;
    }
    ImGui::TextColored(status_color, "%s", status_.c_str());
    if (!marks_valid_ || lasts_.size() != drafts_.size() || risks_.size() != drafts_.size())
    {
        refreshMarks(*store);
    }
    const bool show_add = portfolio_id_ != 0;
    const float footer = show_add ? ImGui::GetFrameHeightWithSpacing() : 0.f;
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float avail = ImGui::GetContentRegionAvail().x;
    const float right_w = std::max(0.f, (avail - gap) * 0.40f);
    const float left_w = std::max(0.f, avail - gap - right_w);
    const ImVec2 pane_size(left_w, footer > 0.f ? -footer : 0.f);
    if (ImGui::BeginChild("portfolio_table", pane_size, ImGuiChildFlags_Borders))
    {
        drawHoldings(*store);
    }
    ImGui::EndChild();
    ImGui::SameLine();
    if (ImGui::BeginChild("portfolio_sizes", ImVec2(0.f, pane_size.y), ImGuiChildFlags_Borders))
    {
        drawAllocation();
    }
    ImGui::EndChild();
    drawAdd(*store, ingest);
    const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
    ImGui::End();
    return focused;
}

}  // namespace terminal
