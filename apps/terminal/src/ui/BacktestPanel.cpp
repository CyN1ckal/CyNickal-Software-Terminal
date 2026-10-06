// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "ui/BacktestPanel.h"

#include "data/IngestWorker.h"
#include "ui/Theme.h"
#include "ui/TradingFormat.h"
#include "ui/TradingWidgets.h"

#include "market_data/NyseCalendar.h"
#include "market_data/Store.h"
#include "market_data/Time.h"

#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <exception>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace terminal {
namespace {

constexpr ChartBarPeriod kPeriods[] = {
    ChartBarPeriod::Day1, ChartBarPeriod::Hour1, ChartBarPeriod::Minute15, ChartBarPeriod::Minute5,
    ChartBarPeriod::Minute1,
};
constexpr const char* kSizingLabels[] = {"Shares", "Dollars", "Percent of equity"};
constexpr int kDailyDefaultDays = 730;
constexpr int kIntradayDefaultDays = 20;
constexpr double kRunsRefreshSeconds = 2.0;

void copyInto(char* buffer, std::size_t size, std::string_view text)
{
    if (size == 0)
    {
        return;
    }
    const std::size_t count = text.copy(buffer, size - 1);
    buffer[count] = '\0';
}

[[nodiscard]] std::string trimmedUpper(const char* text)
{
    std::string out;
    for (const char* ch = text; *ch != '\0'; ++ch)
    {
        if (*ch != ' ' && *ch != '\t')
        {
            out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(*ch))));
        }
    }
    return out;
}

[[nodiscard]] std::string trimmed(const char* text)
{
    std::string_view view(text);
    while (!view.empty() && view.front() == ' ')
    {
        view.remove_prefix(1);
    }
    while (!view.empty() && view.back() == ' ')
    {
        view.remove_suffix(1);
    }
    return std::string(view);
}

[[nodiscard]] SessionDate daysBefore(SessionDate date, int days)
{
    const auto ymd = sessionDateToYmd(date);
    return toSessionDate(std::chrono::sys_days{ymd} - std::chrono::days{days});
}

[[nodiscard]] const char* periodCodeForSeconds(int timeframe_s) noexcept
{
    for (const ChartBarPeriod period : kPeriods)
    {
        if (timeframeSeconds(period) == timeframe_s)
        {
            return chartPeriodCode(period);
        }
    }
    return "?";
}

// Every NYSE session in [from, to] that has ended has a Complete coverage row, as the
// ingest's own check requires, so fetching the range would add nothing. A session that
// is still open or has not started yet is not required.
[[nodiscard]] bool rangeComplete(std::span<const CoverageDay> days,
                                 const Instrument& instrument,
                                 SessionDate from,
                                 SessionDate to,
                                 UnixSeconds now)
{
    std::set<SessionDate> complete;
    for (const CoverageDay& day : days)
    {
        if (day.status == CoverageStatus::Complete)
        {
            complete.insert(day.session_date);
        }
    }
    const SessionDate today = utcToSessionDate(instrument.timezone, now);
    return std::ranges::all_of(nyseSessions(from, to), [&](SessionDate date) {
        return date > today || sessionStillOpen(instrument.timezone, date, now) || complete.contains(date);
    });
}

[[nodiscard]] std::string ledgerDate(UnixSeconds ts)
{
    return formatLedgerTime(ts).substr(0, 10);
}

void sectionHeading(const char* title)
{
    ImGui::Spacing();
    ImGui::TextColored(Theme::accent(), "%s", title);
}

// A labeled double input that keeps the value when the text does not parse.
bool inputNumber(const char* label, double& value, const char* format)
{
    ImGui::TextColored(Theme::textDim(), "%s", label);
    ImGui::SameLine(150.f);
    ImGui::SetNextItemWidth(-1.f);
    const std::string id = std::string("##") + label;
    return ImGui::InputDouble(id.c_str(), &value, 0.0, 0.0, format);
}

}  // namespace

BacktestPanel::BacktestPanel(int id) : id_(id)
{
    const auto types = strategyTypes();
    const StrategyType* preferred = findStrategy("ma_cross");
    if (preferred == nullptr && !types.empty())
    {
        preferred = types.front();
    }
    if (preferred != nullptr)
    {
        selectStrategy(*preferred);
    }
}

BacktestPanel::~BacktestPanel() = default;

int BacktestPanel::id() const noexcept
{
    return id_;
}

bool BacktestPanel::windowOpen() const noexcept
{
    return window_open_;
}

void BacktestPanel::closeWindow()
{
    window_open_ = false;
}

void BacktestPanel::requestFocus()
{
    focus_on_appear_ = true;
}

void BacktestPanel::setWindowScope(int runtime_id) noexcept
{
    runtime_id_ = runtime_id;
}

void BacktestPanel::setPlacement(bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size)
{
    place_force_ = force;
    place_floating_ = floating;
    place_dock_ = dock;
    place_pos_ = pos;
    place_size_ = size;
}

const StrategyType* BacktestPanel::strategy() const noexcept
{
    return findStrategy(strategy_id_);
}

void BacktestPanel::selectStrategy(const StrategyType& type)
{
    strategy_id_ = type.id;
    options_ = clampStrategyOptions(type, {});
}

void BacktestPanel::importState(const ChartbookBacktest& state)
{
    if (const StrategyType* type = findStrategy(state.strategy); type != nullptr)
    {
        selectStrategy(*type);
        if (const auto options = strategyOptionsFromJson(*type, state.params); options.has_value())
        {
            options_ = *options;
        }
    }
    copyInto(symbol_, sizeof(symbol_), state.symbol);
    if (const auto period = backtestPeriodFromCode(state.period); period.has_value())
    {
        period_ = static_cast<int>(*period);
    }
    copyInto(from_, sizeof(from_), state.from);
    copyInto(to_, sizeof(to_), state.to);
    if (const auto config = backtestConfigFromJson(state.config); config.has_value())
    {
        config_ = *config;
        use_stop_ = config_.stop_loss_pct.has_value();
        stop_pct_ = config_.stop_loss_pct.value_or(stop_pct_);
        use_target_ = config_.take_profit_pct.has_value();
        target_pct_ = config_.take_profit_pct.value_or(target_pct_);
    }
}

ChartbookBacktest BacktestPanel::exportState() const
{
    ChartbookBacktest state;
    state.id = id_;
    state.strategy = strategy_id_;
    if (const StrategyType* type = strategy(); type != nullptr)
    {
        state.params = strategyParamsJson(*type, options_);
    }
    state.symbol = symbol_;
    state.period = chartPeriodCode(static_cast<ChartBarPeriod>(period_));
    state.from = from_;
    state.to = to_;
    BacktestConfig config = config_;
    config.stop_loss_pct = use_stop_ ? std::optional<double>(stop_pct_) : std::nullopt;
    config.take_profit_pct = use_target_ ? std::optional<double>(target_pct_) : std::nullopt;
    state.config = backtestConfigJson(config);
    return state;
}

std::optional<BacktestOpenRequest> BacktestPanel::takeOpenRequest()
{
    std::optional<BacktestOpenRequest> request = open_request_;
    open_request_.reset();
    return request;
}

std::unique_ptr<BacktestWorker> BacktestPanel::releaseWorker() noexcept
{
    run_serial_ = 0;
    return std::move(worker_);
}

std::optional<BacktestRequest> BacktestPanel::buildRequest()
{
    const auto fail = [this](std::string message) {
        error_ = std::move(message);
        status_ = error_;
        return std::nullopt;
    };
    const StrategyType* type = strategy();
    if (type == nullptr)
    {
        return fail("pick a strategy");
    }
    BacktestRequest request;
    request.strategy_id = type->id;
    request.options = clampStrategyOptions(*type, options_);
    request.symbol = trimmedUpper(symbol_);
    if (request.symbol.empty())
    {
        return fail("symbol is empty");
    }
    request.period = static_cast<ChartBarPeriod>(period_);
    const bool daily = request.period == ChartBarPeriod::Day1;
    const SessionDate today = utcToSessionDate(kLedgerTimezone, nowUtc());
    const std::string to_text = trimmed(to_);
    const std::string from_text = trimmed(from_);
    const std::optional<SessionDate> to = to_text.empty() ? std::optional<SessionDate>(today) : tryParseIsoDate(to_text);
    if (!to.has_value())
    {
        return fail("To must be YYYY-MM-DD");
    }
    const std::optional<SessionDate> from =
        from_text.empty() ? std::optional<SessionDate>(daysBefore(*to, daily ? kDailyDefaultDays : kIntradayDefaultDays))
                          : tryParseIsoDate(from_text);
    if (!from.has_value())
    {
        return fail("From must be YYYY-MM-DD");
    }
    if (*from > *to)
    {
        return fail("From is after To");
    }
    request.from = *from;
    request.to = *to;
    request.config = config_;
    request.config.stop_loss_pct = use_stop_ ? std::optional<double>(stop_pct_) : std::nullopt;
    request.config.take_profit_pct = use_target_ ? std::optional<double>(target_pct_) : std::nullopt;
    if (daily)
    {
        request.config.flatten_at_session_end = false;
    }
    const BacktestConfig& config = request.config;
    if (!(config.initial_cash > 0.0) || !(config.sizing_value > 0.0))
    {
        return fail("starting cash and position size must be positive");
    }
    if (config.commission_per_share < 0.0 || config.commission_minimum < 0.0 || config.slippage_bps < 0.0)
    {
        return fail("commission and slippage cannot be negative");
    }
    if ((use_stop_ && !(stop_pct_ > 0.0)) || (use_target_ && !(target_pct_ > 0.0)))
    {
        return fail("stop and target percents must be positive");
    }
    error_.clear();
    return request;
}

void BacktestPanel::startRun(const Store& store, IngestWorker* ingest)
{
    std::optional<BacktestRequest> request = buildRequest();
    if (!request.has_value())
    {
        return;
    }
    const int timeframe_s = request->period == ChartBarPeriod::Day1 ? kTimeframe1d : kTimeframe1m;
    std::optional<Instrument> instrument;
    std::vector<CoverageDay> days;
    try
    {
        instrument = store.resolveSymbol(request->symbol);
        if (instrument.has_value())
        {
            days = store.queryCoverageDays(instrument->id, timeframe_s);
        }
    }
    catch (const std::exception& ex)
    {
        // A read can fail on a locked or damaged store. Report it; do not leave the click
        // to unwind out of the frame and take the terminal down with it.
        error_ = ex.what();
        status_ = error_;
        return;
    }
    bool has_bars = false;
    bool complete = false;
    if (instrument.has_value())
    {
        has_bars = std::ranges::any_of(days, [&](const CoverageDay& day) {
            return day.bar_count > 0 && day.session_date >= request->from && day.session_date <= request->to;
        });
        complete = rangeComplete(days, *instrument, request->from, request->to, nowUtc());
    }
    // Run now only when an ingest would add nothing. A partly stored range is fetched
    // first, unless it cannot be (no ingest, or a delisted symbol); then it runs on what is stored.
    const bool can_fetch = ingest != nullptr && (!instrument.has_value() || instrument->listing_open);
    if (has_bars && (complete || !can_fetch))
    {
        enqueueRun(store, std::move(*request));
        return;
    }
    if (!can_fetch)
    {
        error_ = "no " + std::string(chartPeriodCode(request->period)) + " bars for " + request->symbol +
                 " in that range";
        status_ = error_;
        return;
    }
    IngestWorker::Job job;
    job.symbol = request->symbol;
    job.from = request->from;
    job.to = request->to;
    job.timeframe_s = timeframe_s;
    fetch_serial_ = ingest->enqueue(std::move(job)).serial;
    status_ = "fetching " + request->symbol + " bars";
    after_fetch_ = std::move(request);
}

void BacktestPanel::enqueueRun(const Store& store, BacktestRequest request)
{
    if (worker_ == nullptr)
    {
        worker_ = std::make_unique<BacktestWorker>(store.path());
    }
    status_ = "running " + std::string(strategy() != nullptr ? strategy()->display_name : "backtest") + " on " +
              request.symbol;
    outcome_.reset();
    run_serial_ = worker_->enqueue(std::move(request));
}

void BacktestPanel::poll(const Store& store, IngestWorker* ingest)
{
    if (fetch_serial_ != 0 && ingest != nullptr && ingest->snapshot().finished_serial >= fetch_serial_)
    {
        const IngestWorker::SerialFailure failure = ingest->failureForSerial(fetch_serial_);
        fetch_serial_ = 0;
        if (failure.failed)
        {
            error_ = failure.message;
            status_ = error_;
            after_fetch_.reset();
        }
        else if (after_fetch_.has_value())
        {
            BacktestRequest request = std::move(*after_fetch_);
            after_fetch_.reset();
            enqueueRun(store, std::move(request));
        }
    }
    if (run_serial_ != 0 && worker_ != nullptr && worker_->snapshot().finished_serial >= run_serial_)
    {
        outcome_ = worker_->outcome(run_serial_);
        run_serial_ = 0;
        runs_stale_ = true;
        if (outcome_.has_value() && outcome_->ok)
        {
            error_.clear();
            status_ = "recorded " + outcome_->ledger_name;
        }
        else
        {
            error_ = outcome_.has_value() ? outcome_->error : std::string("the backtest did not finish");
            status_ = error_;
        }
    }
}

void BacktestPanel::reloadRuns(const Store& store)
{
    runs_ = store.listBacktestRuns();
    ledger_names_.clear();
    for (const Ledger& ledger : store.listLedgers())
    {
        if (ledger.kind == LedgerKind::Backtest)
        {
            ledger_names_[ledger.id] = ledger.name;
        }
    }
    runs_stale_ = false;
    runs_loaded_at_ = ImGui::GetTime();
}

void BacktestPanel::requestData([[maybe_unused]] Store* store, [[maybe_unused]] IngestWorker* ingest)
{
    runs_stale_ = true;
}

void BacktestPanel::loadInputs(const BacktestRun& run)
{
    if (const StrategyType* type = findStrategy(run.strategy_id); type != nullptr)
    {
        selectStrategy(*type);
        if (const auto options = strategyOptionsFromJson(*type, run.params_json); options.has_value())
        {
            options_ = *options;
        }
    }
    copyInto(symbol_, sizeof(symbol_), run.symbol.value_or(std::string{}));
    for (const ChartBarPeriod period : kPeriods)
    {
        if (timeframeSeconds(period) == run.timeframe_s)
        {
            period_ = static_cast<int>(period);
        }
    }
    copyInto(from_, sizeof(from_), ledgerDate(run.ts_begin));
    copyInto(to_, sizeof(to_), ledgerDate(run.ts_end));
    if (const auto config = backtestConfigFromJson(run.config_json); config.has_value())
    {
        config_ = *config;
        use_stop_ = config_.stop_loss_pct.has_value();
        stop_pct_ = config_.stop_loss_pct.value_or(stop_pct_);
        use_target_ = config_.take_profit_pct.has_value();
        target_pct_ = config_.take_profit_pct.value_or(target_pct_);
    }
    status_ = "loaded the inputs of a recorded run";
}

void BacktestPanel::deleteRun(const Store& store, LedgerId ledger_id)
{
    if (!withWriter(&store, error_, [&](Store& writer) { writer.deleteLedger(ledger_id); }))
    {
        status_ = error_;
        return;
    }
    runs_stale_ = true;
    status_ = "deleted a run";
}

void BacktestPanel::drawInputs(const Store& store, IngestWorker* ingest)
{
    sectionHeading("Strategy");
    const StrategyType* current = strategy();
    ImGui::SetNextItemWidth(-1.f);
    if (ImGui::BeginCombo("##strategy", current != nullptr ? current->display_name : "pick a strategy"))
    {
        for (const StrategyType* type : strategyTypes())
        {
            if (ImGui::Selectable(type->display_name, type == current) && type != current)
            {
                selectStrategy(*type);
            }
        }
        ImGui::EndCombo();
    }
    current = strategy();
    if (current != nullptr)
    {
        if (current->note != nullptr)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, Theme::textDim());
            ImGui::TextWrapped("%s", current->note);
            ImGui::PopStyleColor();
        }
        options_ = clampStrategyOptions(*current, options_);
        for (std::size_t index = 0; index < current->options.size(); ++index)
        {
            const StudyOption& option = current->options[index];
            if (!option.shown)
            {
                continue;
            }
            ImGui::PushID(static_cast<int>(index));
            ImGui::TextColored(Theme::textDim(), "%s", option.label);
            ImGui::SameLine(150.f);
            ImGui::SetNextItemWidth(-1.f);
            int& value = options_[index];
            if (option.choices.empty())
            {
                ImGui::InputInt("##option", &value);
            }
            else if (ImGui::BeginCombo("##option", option.choices[static_cast<std::size_t>(value)].label))
            {
                for (std::size_t choice = 0; choice < option.choices.size(); ++choice)
                {
                    if (ImGui::Selectable(option.choices[choice].label, std::cmp_equal(choice, value)))
                    {
                        value = static_cast<int>(choice);
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::PopID();
        }
        options_ = clampStrategyOptions(*current, options_);
    }

    sectionHeading("Market");
    ImGui::TextColored(Theme::textDim(), "Symbol");
    ImGui::SameLine(150.f);
    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputTextWithHint("##symbol", "AAPL", symbol_, sizeof(symbol_));
    ImGui::TextColored(Theme::textDim(), "Bar period");
    ImGui::SameLine(150.f);
    ImGui::SetNextItemWidth(-1.f);
    if (ImGui::BeginCombo("##period", chartPeriodCode(static_cast<ChartBarPeriod>(period_))))
    {
        for (const ChartBarPeriod period : kPeriods)
        {
            if (ImGui::Selectable(chartPeriodCode(period), static_cast<int>(period) == period_))
            {
                period_ = static_cast<int>(period);
            }
        }
        ImGui::EndCombo();
    }
    const bool daily = static_cast<ChartBarPeriod>(period_) == ChartBarPeriod::Day1;
    ImGui::TextColored(Theme::textDim(), "From");
    ImGui::SameLine(150.f);
    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputTextWithHint("##from", daily ? "two years back" : "20 days back", from_, sizeof(from_));
    ImGui::TextColored(Theme::textDim(), "To");
    ImGui::SameLine(150.f);
    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputTextWithHint("##to", "today", to_, sizeof(to_));

    sectionHeading("Sizing and costs");
    inputNumber("Starting cash", config_.initial_cash, "%.2f");
    ImGui::TextColored(Theme::textDim(), "Size by");
    ImGui::SameLine(150.f);
    ImGui::SetNextItemWidth(-1.f);
    int sizing = static_cast<int>(config_.sizing);
    if (ImGui::Combo("##sizing", &sizing, kSizingLabels, 3))
    {
        config_.sizing = static_cast<BacktestSizing>(sizing);
    }
    inputNumber("Size per unit", config_.sizing_value, "%.2f");
    inputNumber("Commission / share", config_.commission_per_share, "%.4f");
    inputNumber("Commission minimum", config_.commission_minimum, "%.2f");
    inputNumber("Slippage (bps)", config_.slippage_bps, "%.1f");

    sectionHeading("Exits");
    ImGui::Checkbox("Stop loss %", &use_stop_);
    ImGui::SameLine(150.f);
    ImGui::SetNextItemWidth(-1.f);
    ImGui::BeginDisabled(!use_stop_);
    ImGui::InputDouble("##stop", &stop_pct_, 0.0, 0.0, "%.2f");
    ImGui::EndDisabled();
    ImGui::Checkbox("Take profit %", &use_target_);
    ImGui::SameLine(150.f);
    ImGui::SetNextItemWidth(-1.f);
    ImGui::BeginDisabled(!use_target_);
    ImGui::InputDouble("##target", &target_pct_, 0.0, 0.0, "%.2f");
    ImGui::EndDisabled();
    ImGui::BeginDisabled(daily);
    ImGui::Checkbox("Flat at each session close", &config_.flatten_at_session_end);
    ImGui::EndDisabled();
    ImGui::Checkbox("Close at the last bar", &config_.close_at_end);

    ImGui::Spacing();
    const bool busy = run_serial_ != 0 || fetch_serial_ != 0;
    ImGui::BeginDisabled(busy);
    if (primaryButton("Run Backtest"))
    {
        startRun(store, ingest);
    }
    ImGui::EndDisabled();
}

void BacktestPanel::drawRuns(const Store& store)
{
    if (outcome_.has_value() && outcome_->ok)
    {
        ImGui::TextUnformatted(outcome_->ledger_name.c_str());
        ImGui::SameLine();
        ImGui::TextColored(Theme::textDim(), "%zu bars, %zu fills, final equity", outcome_->bars, outcome_->fills);
        ImGui::SameLine();
        ImGui::TextColored(outcome_->total_return < 0.0 ? Theme::down() : Theme::up(), "%s (%s)",
                           formatMoney(outcome_->final_equity).c_str(), formatPercent(outcome_->total_return).c_str());
        ImGui::SameLine();
        if (primaryButton("Open in Statistics"))
        {
            open_request_ = BacktestOpenRequest{.statistics = true, .ledger_id = outcome_->recorded.ledger_id};
        }
        ImGui::SameLine();
        if (ImGui::Button("Open Ledger"))
        {
            open_request_ = BacktestOpenRequest{.statistics = false, .ledger_id = outcome_->recorded.ledger_id};
        }
        ImGui::Separator();
    }
    if (runs_.empty())
    {
        ImGui::TextColored(Theme::textFaint(), "No recorded runs yet.");
        return;
    }
    constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                                      ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollY |
                                      ImGuiTableFlags_ScrollX | ImGuiTableFlags_Resizable |
                                      ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings;
    if (!ImGui::BeginTable("backtest_runs", 7, flags))
    {
        return;
    }
    for (const char* header : {"Run", "Symbol", "Period", "From", "To", "Parameters", "Recorded"})
    {
        ImGui::TableSetupColumn(header);
    }
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();
    std::optional<LedgerId> doomed;
    for (const BacktestRun& run : runs_)
    {
        ImGui::TableNextRow();
        ImGui::PushID(static_cast<int>(run.id));
        ImGui::TableSetColumnIndex(0);
        const auto name = ledger_names_.find(run.ledger_id);
        const std::string label = name != ledger_names_.end() ? name->second : run.strategy_id;
        if (ImGui::Selectable(label.c_str(), false,
                              ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap |
                                  ImGuiSelectableFlags_AllowDoubleClick) &&
            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            open_request_ = BacktestOpenRequest{.statistics = true, .ledger_id = run.ledger_id};
        }
        if (ImGui::BeginPopupContextItem("run_menu"))
        {
            if (ImGui::MenuItem("Open in Statistics"))
            {
                open_request_ = BacktestOpenRequest{.statistics = true, .ledger_id = run.ledger_id};
            }
            if (ImGui::MenuItem("Open Ledger"))
            {
                open_request_ = BacktestOpenRequest{.statistics = false, .ledger_id = run.ledger_id};
            }
            if (ImGui::MenuItem("Load Inputs"))
            {
                loadInputs(run);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete Run..."))
            {
                doomed = run.ledger_id;
            }
            ImGui::EndPopup();
        }
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(run.symbol.has_value() ? run.symbol->c_str() : "");
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(periodCodeForSeconds(run.timeframe_s));
        ImGui::TableSetColumnIndex(3);
        ImGui::TextUnformatted(ledgerDate(run.ts_begin).c_str());
        ImGui::TableSetColumnIndex(4);
        ImGui::TextUnformatted(ledgerDate(run.ts_end).c_str());
        ImGui::TableSetColumnIndex(5);
        ImGui::TextColored(Theme::textDim(), "%s", run.params_json.c_str());
        ImGui::TableSetColumnIndex(6);
        ImGui::TextColored(Theme::textDim(), "%s", formatLedgerTime(run.created_at).c_str());
        ImGui::PopID();
    }
    ImGui::EndTable();

    if (doomed.has_value())
    {
        delete_ledger_ = *doomed;
        ImGui::OpenPopup("Delete run");
    }
    if (const ImGuiViewport* viewport = ImGui::GetMainViewport(); viewport != nullptr)
    {
        // The pivot is a fraction of the window, not a layout length: px() would move the dialog.
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    }
    if (ImGui::BeginPopupModal("Delete run", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        const auto name = ledger_names_.find(delete_ledger_);
        ImGui::TextUnformatted("Delete this backtest run?");
        ImGui::TextUnformatted(name != ledger_names_.end() ? name->second.c_str() : "");
        ImGui::TextUnformatted("Its ledger, fills, and run record are removed.");
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::cancel());
        if (ImGui::Button("Delete"))
        {
            deleteRun(store, delete_ledger_);
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        ImGui::EndPopup();
    }
}

bool BacktestPanel::draw(Store* store, std::string_view store_error, IngestWorker* ingest)
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

    const std::string title = "BACKTEST###cb" + std::to_string(runtime_id_) + "_backtest" + std::to_string(id_);
    if (!ImGui::Begin(title.c_str(), &window_open_, ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::End();
        return false;
    }
    if (store == nullptr)
    {
        if (!store_error.empty())
        {
            ImGui::TextColored(Theme::danger(), "%.*s", static_cast<int>(store_error.size()), store_error.data());
        }
        ImGui::End();
        return ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    }

    try
    {
        poll(*store, ingest);
        if (runs_stale_ || ImGui::GetTime() - runs_loaded_at_ > kRunsRefreshSeconds)
        {
            reloadRuns(*store);
        }
    }
    catch (const std::exception& ex)
    {
        error_ = ex.what();
        status_ = error_;
        runs_stale_ = false;
        runs_loaded_at_ = ImGui::GetTime();
    }

    ImVec4 status_color = Theme::muted();
    if (run_serial_ != 0 || fetch_serial_ != 0)
    {
        status_color = Theme::accent();
    }
    else if (!error_.empty())
    {
        status_color = Theme::danger();
    }
    if (status_.empty())
    {
        ImGui::TextColored(Theme::textFaint(), "%s",
                           std::string_view(symbol_).empty() ? "Pick a strategy and a symbol, then Run Backtest."
                                               : "Ready. Missing bars are downloaded before the run.");
    }
    else
    {
        ImGui::TextColored(status_color, "%s", status_.c_str());
    }
    ImGui::Separator();
    if (ImGui::BeginChild("backtest_inputs", ImVec2(Theme::px(340.f), 0.f), ImGuiChildFlags_Borders))
    {
        drawInputs(*store, ingest);
    }
    ImGui::EndChild();
    ImGui::SameLine();
    if (ImGui::BeginChild("backtest_runs_pane", ImVec2(0.f, 0.f), ImGuiChildFlags_Borders))
    {
        drawRuns(*store);
    }
    ImGui::EndChild();
    const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
    ImGui::End();
    return focused;
}

}  // namespace terminal
