// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "ui/TradingFormat.h"

#include <limits>

namespace {

// 2026-01-15 21:00 UTC is 16:00 EST. 2026-07-15 20:00 UTC is 16:00 EDT.
constexpr terminal::UnixSeconds kWinterClose = 1'768'510'800;
constexpr terminal::UnixSeconds kSummerClose = 1'784'145'600;

}  // namespace

TEST_CASE("money, quantity, price, and percent text")
{
    CHECK(terminal::formatMoney(1234.5) == "$1,234.50");
    CHECK(terminal::formatMoney(-1234567.891) == "-$1,234,567.89");
    CHECK(terminal::formatMoney(-0.001) == "$0.00");
    CHECK(terminal::formatMoney(12.0, 0) == "$12");
    CHECK(terminal::formatMoney(std::numeric_limits<double>::quiet_NaN()).empty());
    CHECK(terminal::formatQuantity(12.5) == "12.5");
    CHECK(terminal::formatQuantity(-3.0) == "-3");
    CHECK(terminal::formatQuantity(1000.25) == "1,000.25");
    CHECK(terminal::formatPrice(0.0525) == "0.0525");
    CHECK(terminal::formatPrice(1523.1) == "1,523.10");
    CHECK(terminal::formatPercent(0.1234) == "12.34%");
    CHECK(terminal::formatPercent(-0.05, 1) == "-5.0%");
}

TEST_CASE("ledger times are New York local")
{
    CHECK(terminal::formatLedgerTime(kWinterClose) == "2026-01-15 16:00");
    CHECK(terminal::formatLedgerTime(kSummerClose) == "2026-07-15 16:00");
    CHECK(terminal::formatDuration(0) == "0m");
    CHECK(terminal::formatDuration(42 * 60) == "42m");
    CHECK(terminal::formatDuration((5 * 3'600) + (12 * 60)) == "5h 12m");
    CHECK(terminal::formatDuration((3 * 86'400) + (4 * 3'600)) == "3d 4h");
}

TEST_CASE("typed ledger times read as New York time and a date is its close")
{
    constexpr terminal::UnixSeconds now = 1'700'000'000;
    CHECK(terminal::parseLedgerTime("", now) == now);
    CHECK(terminal::parseLedgerTime("  ", now) == now);
    CHECK(terminal::parseLedgerTime("2026-01-15", now) == kWinterClose);
    CHECK(terminal::parseLedgerTime("20260715", now) == kSummerClose);
    CHECK(terminal::parseLedgerTime("2026-07-15 09:30", now) == kSummerClose - (6 * 3'600) - (30 * 60));
    CHECK(terminal::parseLedgerTime("2026-01-15 16:00:30", now) == kWinterClose + 30);
    CHECK_FALSE(terminal::parseLedgerTime("2026-02-30", now).has_value());
    CHECK_FALSE(terminal::parseLedgerTime("yesterday", now).has_value());
    CHECK_FALSE(terminal::parseLedgerTime("2026-01-15T16:00", now).has_value());
}

TEST_CASE("typed ledger numbers allow separators and a dollar sign")
{
    CHECK(terminal::parseLedgerNumber("1,234.5") == 1234.5);
    CHECK(terminal::parseLedgerNumber("$12") == 12.0);
    CHECK(terminal::parseLedgerNumber("-$3.25") == -3.25);
    CHECK(terminal::parseLedgerNumber(" 7 ") == 7.0);
    CHECK_FALSE(terminal::parseLedgerNumber("").has_value());
    CHECK_FALSE(terminal::parseLedgerNumber("-").has_value());
    CHECK_FALSE(terminal::parseLedgerNumber("--1").has_value());
    CHECK_FALSE(terminal::parseLedgerNumber("12abc").has_value());
    CHECK_FALSE(terminal::parseLedgerNumber("inf").has_value());
}

TEST_CASE("contract labels name an option and leave shares blank")
{
    terminal::PositionKey key;
    CHECK(terminal::contractLabel(key).empty());
    key.kind = terminal::TradeAssetKind::Option;
    key.expiration = 20261016;
    key.strike = 5800;
    key.right = terminal::OptionRight::Put;
    CHECK(terminal::contractLabel(key) == "2026-10-16 5800P");
    key.strike = 212.5;
    key.right = terminal::OptionRight::Call;
    CHECK(terminal::contractLabel(key) == "2026-10-16 212.5C");
}
