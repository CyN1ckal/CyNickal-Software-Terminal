// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "market_data/Types.h"

#include <optional>
#include <string>

namespace terminal {

struct MboumV3BarRow
{
    std::string datetime;
    double open{};
    double high{};
    double low{};
    double close{};
    double volume{};
};

struct MboumV2BarRow
{
    std::string timestamp;
    std::optional<UnixSeconds> timestamp_unix;
    double open{};
    double high{};
    double low{};
    double close{};
    double volume{};
};

std::optional<Bar> mapV3Bar(const Instrument& inst, const MboumV3BarRow& row, UnixSeconds now_utc);
std::optional<Bar> mapV2Bar(const Instrument& inst, const MboumV2BarRow& row, UnixSeconds now_utc);

struct MboumSplitRow
{
    std::string ticker;
    std::string startdatetime;
    std::optional<double> old_share_worth;
    std::optional<double> share_worth;
};

struct MboumDividendRow
{
    std::string symbol;
    std::string ex_date;
    std::string amount;
    std::optional<std::string> currency;
};

std::optional<CorporateAction> mapSplit(InstrumentId id, const MboumSplitRow& row);
std::optional<CorporateAction> mapDividend(InstrumentId id, const MboumDividendRow& row);

}  // namespace terminal
