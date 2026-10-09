// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "market_data/Types.h"

#include <compare>
#include <cstddef>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace terminal {

// What one lot is a position in. Shares of one instrument share a key. An option
// is further keyed by its contract, so a call spread is two positions.
// Share keys leave the option fields at their defaults.
struct PositionKey
{
    InstrumentId instrument_id{};
    TradeAssetKind kind{TradeAssetKind::Equity};
    SessionDate expiration{};
    OptionExpirationType expiration_type{OptionExpirationType::Weekly};
    double strike{};
    OptionRight right{OptionRight::Call};

    [[nodiscard]] friend bool operator==(const PositionKey&, const PositionKey&) = default;
    [[nodiscard]] friend auto operator<=>(const PositionKey&, const PositionKey&) = default;
};

// Fills whose quantity is within this of zero after matching are closed. It absorbs
// the rounding of fractional shares, not a real remainder.
inline constexpr double kLotQuantityEpsilon = 1e-9;

// Units of the underlying per unit of quantity: 1 for a share, 100 for a contract.
[[nodiscard]] double positionMultiplier(TradeAssetKind kind) noexcept;

// The key a fill trades. Requires fill.instrument_id (set on every row Store reads).
[[nodiscard]] PositionKey positionKey(const TradeFill& fill);

// Cash a fill moves: -quantity * price * multiplier - fees. A buy is negative.
[[nodiscard]] double fillCashFlow(const TradeFill& fill);

// The unmatched part of one opening fill. quantity is signed and, like price,
// is in post-split units once a split has been applied. fees is the part of the
// opening fill's fees that no close has taken yet.
struct OpenLot
{
    PositionKey key;
    std::string symbol;
    TradeFillId fill_id{};
    UnixSeconds opened_at{};
    double quantity{};
    double price{};
    double fees{};
};

// One matched open and close. quantity is signed: positive for a long that was
// sold, negative for a short that was covered. fees is the matched share of the
// opening fill's fees plus the matched share of the closing fill's fees.
struct RoundTrip
{
    PositionKey key;
    std::string symbol;
    TradeFillId open_fill_id{};
    TradeFillId close_fill_id{};
    UnixSeconds opened_at{};
    UnixSeconds closed_at{};
    double quantity{};
    double entry_price{};
    double exit_price{};
    double multiplier{1.0};
    double gross_pnl{};
    double fees{};
    double net_pnl{};
};

// Open lots of one key, summed. average_price is weighted by quantity.
// cost_basis is quantity * average_price * multiplier, signed like quantity.
struct Position
{
    PositionKey key;
    std::string symbol;
    double quantity{};
    double average_price{};
    double multiplier{1.0};
    double cost_basis{};
    double open_fees{};
    std::size_t lots{};
};

// Profit of a position marked at mark, before fees: quantity * (mark - average) * multiplier.
[[nodiscard]] double unrealizedPnl(const Position& position, double mark) noexcept;

// First-in, first-out lot matching over time. A fill first closes the oldest open
// lots of its key on the other side, then opens a lot with whatever is left, so
// one fill can close a long and open a short. Splits (CorporateActionType::Split
// with a positive split_ratio) rescale open share lots on their ex_ts: quantity is
// multiplied by the ratio and price divided by it. A split whose ex_ts equals a
// fill's ts is applied before that fill, as adjustBarsForSplits treats the first
// post-split bar. Option lots are not split-adjusted. A cash dividend
// (CorporateActionType::Dividend with a finite, non-negative amount) is credited
// once, at its ex_ts, as open share quantity times amount. Splits at that same
// ex_ts are applied first. A fill at that ex_ts is matched afterwards, so it does
// not receive the dividend. Options and a flat position receive nothing.
class LotBook
{
public:
    LotBook() = default;
    explicit LotBook(std::span<const CorporateAction> actions);

    // Applies every split and dividend with ex_ts <= ts. Throws when ts is earlier than a time already reached.
    void advanceTo(UnixSeconds ts);
    // advanceTo(fill.ts), then matches. Throws on a fill with no instrument_id or a
    // non-finite or zero quantity, a non-finite or negative price, or non-finite or negative fees.
    void apply(const TradeFill& fill);

    // Opening order.
    [[nodiscard]] const std::vector<OpenLot>& openLots() const noexcept;
    // Closing order.
    [[nodiscard]] const std::vector<RoundTrip>& roundTrips() const noexcept;
    // One row per open key, ordered by key.
    [[nodiscard]] std::vector<Position> positions() const;
    // Sum of RoundTrip::net_pnl.
    [[nodiscard]] double realizedPnl() const noexcept;
    // Every fee applied so far, matched or still on an open lot.
    [[nodiscard]] double feesPaid() const noexcept;
    // Sum of fillCashFlow over every fill applied. Dividends are not included.
    [[nodiscard]] double tradeCash() const noexcept;
    // Cash dividends credited up to the book's time.
    [[nodiscard]] double dividendCash() const noexcept;
    [[nodiscard]] UnixSeconds time() const noexcept;

private:
    struct Split
    {
        UnixSeconds ex_ts{};
        InstrumentId instrument_id{};
        double ratio{1.0};
    };

    struct Dividend
    {
        UnixSeconds ex_ts{};
        InstrumentId instrument_id{};
        double amount{};
    };

    std::vector<Split> splits_;
    std::vector<Dividend> dividends_;
    std::size_t next_split_{0};
    std::size_t next_dividend_{0};
    std::vector<OpenLot> lots_;
    std::vector<RoundTrip> trips_;
    UnixSeconds time_{std::numeric_limits<UnixSeconds>::min()};
    double realized_{0.0};
    double fees_{0.0};
    double cash_{0.0};
    double dividend_cash_{0.0};
};

struct LedgerBook
{
    std::vector<OpenLot> open_lots;
    std::vector<RoundTrip> round_trips;
    std::vector<Position> positions;
    double realized_pnl{};
    double fees_paid{};
    double trade_cash{};
    double dividend_cash{};
};

// Sorts fills by ts (ties keep input order), applies them to a LotBook, then
// advances to as_of so later splits reach the lots still open. An as_of earlier
// than the last fill stops at the last fill.
[[nodiscard]] LedgerBook matchLots(std::span<const TradeFill> fills,
                                   std::span<const CorporateAction> actions,
                                   UnixSeconds as_of);

}  // namespace terminal
