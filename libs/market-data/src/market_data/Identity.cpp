// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "market_data/Identity.h"

#include "market_data/Time.h"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <utility>

namespace terminal {
namespace {

[[nodiscard]] std::string lastVerified(const std::optional<UnixSeconds>& verified_at)
{
    if (!verified_at.has_value())
    {
        return "never";
    }
    return formatSessionDate(utcToSessionDate("America/New_York", *verified_at));
}

// What a closed listing on symbol says about where its security went.
[[nodiscard]] std::optional<Instrument> formerHolder(const Store& store, std::string_view symbol)
{
    const auto closed = store.latestClosedListing(symbol);
    if (!closed.has_value() || closed->close_reason != ListingCloseReason::Renamed)
    {
        return std::nullopt;
    }
    return store.findInstrumentById(closed->instrument_id);
}

[[nodiscard]] ResolvedInstrument resolveUnlisted(Store& store,
                                                 OpenFigiClient& figi,
                                                 const std::string& symbol,
                                                 UnixSeconds now)
{
    const ForwardOutcome forward = figi.forward(symbol);
    switch (forward.kind)
    {
    case ForwardKind::Confirmed:
    {
        if (const auto known = store.findInstrumentByFigi(forward.figi))
        {
            ResolvedInstrument out;
            out.id = known->id;
            if (known->listing_open)
            {
                store.relinkSymbol(known->id, symbol, now);
                out.notice = symbol + ": " + known->symbol + " was renamed to " + symbol;
            }
            else
            {
                store.openListing(known->id, symbol, now);
            }
            store.markVerified(known->id, now);
            return out;
        }
        Instrument instrument;
        instrument.symbol = symbol;
        instrument.figi = forward.figi;
        instrument.asset_class = forward.asset_class;
        if (!forward.name.empty())
        {
            instrument.name = forward.name;
        }
        instrument.verified_at = now;
        ResolvedInstrument out;
        out.id = store.insertInstrument(instrument, now);
        if (const auto holder = formerHolder(store, symbol))
        {
            out.notice = symbol + " now names " + (forward.name.empty() ? forward.figi : forward.name) +
                         "; the former " + symbol + " trades as " + holder->symbol;
        }
        return out;
    }
    case ForwardKind::NoMatch:
    {
        if (const auto closed = store.latestClosedListing(symbol))
        {
            const auto holder = store.findInstrumentById(closed->instrument_id);
            if (closed->close_reason == ListingCloseReason::Renamed && holder.has_value())
            {
                if (holder->listing_open)
                {
                    throw std::runtime_error(symbol + " is no longer listed; that security now trades as " +
                                             holder->symbol);
                }
                if (!sameTicker(holder->symbol, symbol))
                {
                    throw std::runtime_error(symbol + " is no longer listed; that security was renamed to " +
                                             holder->symbol);
                }
                // The rename was found but its new ticker is open on another instrument.
                throw std::runtime_error(symbol + " identity conflict: its new ticker is open on another "
                                                  "instrument; run ingest --verify, or ingest --delist the other one");
            }
            if (closed->close_reason == ListingCloseReason::Delisted)
            {
                throw std::runtime_error(symbol + " is delisted");
            }
        }
        throw std::runtime_error(symbol + " has no US listing in OpenFIGI");
    }
    case ForwardKind::Ambiguous:
        throw std::runtime_error(symbol + " matches more than one security in OpenFIGI (" + forward.message + ")");
    case ForwardKind::Unsupported:
        throw std::runtime_error(symbol + " is not an equity, ETF, or index (OpenFIGI: " + forward.message + ")");
    case ForwardKind::Unreachable:
        break;
    }
    throw std::runtime_error("cannot confirm " + symbol + ": OpenFIGI unreachable (" + forward.message + ")");
}

struct Pending
{
    std::size_t report{};
    Instrument instrument;
    std::string figi;                       // instrument.figi, known to be set
    std::optional<std::string> other_figi;  // forward returned a different security
};

}  // namespace

std::string_view toString(VerifyOutcome outcome) noexcept
{
    switch (outcome)
    {
    case VerifyOutcome::Ok:
        return "ok";
    case VerifyOutcome::Renamed:
        return "renamed";
    case VerifyOutcome::Delisted:
        return "delisted";
    case VerifyOutcome::Conflict:
        return "conflict";
    case VerifyOutcome::Unreachable:
        return "unreachable";
    case VerifyOutcome::Unresolved:
        return "unresolved";
    case VerifyOutcome::Skipped:
        return "skipped";
    }
    return "unknown";
}

std::string describe(const VerifyReport& report)
{
    switch (report.outcome)
    {
    case VerifyOutcome::Renamed:
        return "renamed " + report.symbol + " -> " + report.new_symbol;
    case VerifyOutcome::Conflict:
    case VerifyOutcome::Unreachable:
    case VerifyOutcome::Unresolved:
    case VerifyOutcome::Skipped:
        return std::string(toString(report.outcome)) + (report.detail.empty() ? "" : ": " + report.detail);
    case VerifyOutcome::Ok:
    case VerifyOutcome::Delisted:
        break;
    }
    return std::string(toString(report.outcome));
}

ResolvedInstrument ensureInstrument(Store& store, OpenFigiClient& figi, std::string_view symbol_in, UnixSeconds now)
{
    const std::string symbol = canonicalListingSymbol(symbol_in);
    if (symbol.empty())
    {
        throw std::runtime_error("ingest symbol is empty");
    }
    if (const auto open = store.findOpenListing(symbol))
    {
        if (!open->figi.has_value())
        {
            return ResolvedInstrument{.id = open->id, .notice = {}};
        }
        if (open->verified_at.has_value() && now - *open->verified_at < kVerifyMaxAge)
        {
            return ResolvedInstrument{.id = open->id, .notice = {}};
        }
        const InstrumentId id = open->id;
        const std::vector<VerifyReport> reports = verifyIdentities(store, figi, now, std::span(&id, 1));
        const VerifyReport& report = reports.front();
        switch (report.outcome)
        {
        case VerifyOutcome::Ok:
        case VerifyOutcome::Skipped:
            return ResolvedInstrument{.id = id, .notice = {}};
        case VerifyOutcome::Unreachable:
            if (open->verified_at.has_value() && now - *open->verified_at <= kVerifyGrace)
            {
                return ResolvedInstrument{
                    .id = id,
                    .notice = "warning: could not confirm " + symbol + " with OpenFIGI (" + report.detail +
                              "); last verified " + lastVerified(open->verified_at),
                };
            }
            throw std::runtime_error("cannot confirm " + symbol + ": OpenFIGI unreachable, last verified " +
                                     lastVerified(open->verified_at));
        case VerifyOutcome::Unresolved:
            // OpenFIGI has said this ticker no longer names the stored FIGI. Grace does not apply.
            throw std::runtime_error("cannot confirm " + symbol + ": " + report.detail);
        case VerifyOutcome::Conflict:
            throw std::runtime_error(symbol + " identity conflict: " + report.detail);
        case VerifyOutcome::Renamed:
        case VerifyOutcome::Delisted:
            break;  // the listing is closed now
        }
    }
    return resolveUnlisted(store, figi, symbol, now);
}

std::vector<VerifyReport> verifyIdentities(Store& store,
                                           OpenFigiClient& figi,
                                           UnixSeconds now,
                                           std::span<const InstrumentId> ids)
{
    std::vector<VerifyReport> reports(ids.size());
    std::vector<Pending> candidates;
    for (std::size_t i = 0; i < ids.size(); ++i)
    {
        reports[i].id = ids[i];
        const auto instrument = store.findInstrumentById(ids[i]);
        if (!instrument.has_value())
        {
            reports[i].detail = "unknown instrument";
            continue;
        }
        reports[i].symbol = instrument->symbol;
        if (!instrument->figi.has_value() || !instrument->listing_open)
        {
            reports[i].detail = instrument->figi.has_value() ? "no open listing" : "no FIGI";
            continue;
        }
        candidates.push_back(Pending{.report = i,
                                     .instrument = *instrument,
                                     .figi = instrument->figi.value_or(std::string{}),
                                     .other_figi = std::nullopt,});
    }

    // Pass 1: the open symbol forward.
    std::vector<OpenFigiJob> jobs;
    jobs.reserve(candidates.size());
    for (const Pending& row : candidates)
    {
        jobs.push_back(forwardJob(row.instrument.symbol));
    }
    std::vector<OpenFigiJobResult> results = figi.map(jobs);
    std::vector<Pending> reverse;
    for (std::size_t i = 0; i < candidates.size(); ++i)
    {
        Pending& row = candidates[i];
        VerifyReport& report = reports[row.report];
        const ForwardOutcome forward = reduceForward(results[i], isIndexSymbol(row.instrument.symbol));
        if (forward.kind == ForwardKind::Confirmed && forward.figi == row.figi)
        {
            report.outcome = VerifyOutcome::Ok;
            continue;
        }
        if (forward.kind == ForwardKind::Unreachable)
        {
            report.outcome = VerifyOutcome::Unreachable;
            report.detail = forward.message;
            continue;
        }
        if (forward.kind == ForwardKind::Confirmed)
        {
            row.other_figi = forward.figi;
        }
        reverse.push_back(row);
    }

    // Pass 2: the stored FIGI back to its ticker.
    jobs.clear();
    for (const Pending& row : reverse)
    {
        jobs.push_back(reverseJob(row.figi));
    }
    results = figi.map(jobs);
    std::vector<Pending> confirm;
    std::vector<std::string> confirm_tickers;
    for (std::size_t i = 0; i < reverse.size(); ++i)
    {
        const Pending& row = reverse[i];
        VerifyReport& report = reports[row.report];
        const ReverseOutcome back = reduceReverse(results[i]);
        switch (back.kind)
        {
        case ReverseKind::Ticker:
            if (sameTicker(back.ticker, row.instrument.symbol))
            {
                report.outcome = VerifyOutcome::Delisted;
                report.detail = row.other_figi.has_value() ? row.instrument.symbol + " now names " + *row.other_figi
                                                           : row.instrument.symbol + " is no longer listed";
            }
            else
            {
                confirm.push_back(row);
                confirm_tickers.push_back(back.ticker);
            }
            break;
        case ReverseKind::NoMatch:
            report.outcome = VerifyOutcome::Conflict;
            report.detail = "OpenFIGI no longer knows " + row.figi;
            break;
        case ReverseKind::Ambiguous:
            report.outcome = VerifyOutcome::Conflict;
            report.detail = row.figi + " maps to " + back.message;
            break;
        case ReverseKind::Unreachable:
            report.outcome = VerifyOutcome::Unresolved;
            report.detail = row.instrument.symbol + " no longer maps to " + row.figi +
                            "; the reverse lookup failed (" + back.message + ")";
            break;
        }
    }

    // Pass 3: the new ticker forward, to tell a live rename from a rename that later delisted.
    jobs.clear();
    for (const std::string& ticker : confirm_tickers)
    {
        jobs.push_back(forwardJob(ticker));
    }
    results = figi.map(jobs);
    for (std::size_t i = 0; i < confirm.size(); ++i)
    {
        const Pending& row = confirm[i];
        VerifyReport& report = reports[row.report];
        const ForwardOutcome forward = reduceForward(results[i], isIndexSymbol(confirm_tickers[i]));
        if (forward.kind == ForwardKind::Unreachable)
        {
            report.outcome = VerifyOutcome::Unresolved;
            report.detail = row.instrument.symbol + " no longer maps to " + row.figi + "; confirming " +
                            confirm_tickers[i] + " failed (" + forward.message + ")";
        }
        else if (forward.kind == ForwardKind::Confirmed && forward.figi == row.figi)
        {
            report.outcome = VerifyOutcome::Renamed;
            report.new_symbol = confirm_tickers[i];
        }
        else
        {
            report.outcome = VerifyOutcome::Delisted;
            report.detail = "renamed to " + confirm_tickers[i] + ", which is no longer listed";
        }
    }

    // Apply: verification stamps, then closes, then opens.
    std::vector<ListingChange> changes;
    std::vector<std::size_t> open_report;
    for (const VerifyReport& report : reports)
    {
        if (report.outcome == VerifyOutcome::Ok || report.outcome == VerifyOutcome::Renamed ||
            report.outcome == VerifyOutcome::Delisted)
        {
            changes.push_back(ListingChange{.kind = ListingChange::Kind::MarkVerified,
                                            .instrument_id = report.id,
                                            .reason = ListingCloseReason::Manual,
                                            .symbol = {},});
        }
        else if (report.outcome == VerifyOutcome::Conflict)
        {
            changes.push_back(ListingChange{.kind = ListingChange::Kind::ClearVerified,
                                            .instrument_id = report.id,
                                            .reason = ListingCloseReason::Manual,
                                            .symbol = {},});
        }
    }
    for (const VerifyReport& report : reports)
    {
        if (report.outcome == VerifyOutcome::Renamed || report.outcome == VerifyOutcome::Delisted)
        {
            changes.push_back(ListingChange{.kind = ListingChange::Kind::Close,
                                            .instrument_id = report.id,
                                            .reason = report.outcome == VerifyOutcome::Renamed
                                                          ? ListingCloseReason::Renamed
                                                          : ListingCloseReason::Delisted,
                                            .symbol = {},});
        }
    }
    for (std::size_t i = 0; i < reports.size(); ++i)
    {
        if (reports[i].outcome == VerifyOutcome::Renamed)
        {
            open_report.push_back(i);
            changes.push_back(ListingChange{.kind = ListingChange::Kind::Open,
                                            .instrument_id = reports[i].id,
                                            .reason = ListingCloseReason::Manual,
                                            .symbol = reports[i].new_symbol,});
        }
    }
    if (!changes.empty())
    {
        const std::size_t first_open = changes.size() - open_report.size();
        for (const std::size_t skipped : store.applyListingChanges(changes, now))
        {
            VerifyReport& report = reports[open_report[skipped - first_open]];
            report.outcome = VerifyOutcome::Conflict;
            report.detail = "renamed to " + report.new_symbol + ", which is open on another instrument";
        }
    }
    return reports;
}

std::vector<InstrumentId> instrumentsDueForVerification(const Store& store, UnixSeconds now, bool all)
{
    std::vector<InstrumentId> out;
    for (const Instrument& instrument : store.listInstruments())
    {
        if (!instrument.figi.has_value() || !instrument.listing_open)
        {
            continue;
        }
        if (all || !instrument.verified_at.has_value() || now - *instrument.verified_at >= kVerifyMaxAge)
        {
            out.push_back(instrument.id);
        }
    }
    return out;
}

}  // namespace terminal
