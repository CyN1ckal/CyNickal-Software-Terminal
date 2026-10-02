// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "market_data/Types.h"
#include "trading/Ledger.h"

#include <optional>
#include <string>
#include <string_view>

namespace terminal {

// Text for the ledger, statistics, and backtest panels. No ImGui.

// Ledger times are shown and typed in New York time.
inline constexpr std::string_view kLedgerTimezone = "America/New_York";

// Fixed-point with thousands separators: "-$1,234.50". Non-finite is "".
[[nodiscard]] std::string formatMoney(double amount, int decimals = 2);

// Up to four places, trailing zeros dropped: "12.5", "-3".
[[nodiscard]] std::string formatQuantity(double quantity);

// Four places under a dollar, two otherwise, with separators and no sign of currency.
[[nodiscard]] std::string formatPrice(double price);

// A fraction as a percent: 0.1234 -> "12.34%". Non-finite is "".
[[nodiscard]] std::string formatPercent(double fraction, int decimals = 2);

// "YYYY-MM-DD HH:MM" in New York time.
[[nodiscard]] std::string formatLedgerTime(UnixSeconds ts);

// "3d 4h", "5h 12m", "42m", "0m".
[[nodiscard]] std::string formatDuration(UnixSeconds seconds);

// Empty text is now. "YYYY-MM-DD" or "YYYYMMDD" is that day's 16:00 close in New
// York. "YYYY-MM-DD HH:MM[:SS]" is that New York time. Anything else is nullopt.
[[nodiscard]] std::optional<UnixSeconds> parseLedgerTime(std::string_view text, UnixSeconds now);

// A finite number; commas, spaces, and a leading '$' are ignored.
[[nodiscard]] std::optional<double> parseLedgerNumber(std::string_view text);

// "2026-10-16 5800P" for an option key, "" for shares.
[[nodiscard]] std::string contractLabel(const PositionKey& key);

}  // namespace terminal
