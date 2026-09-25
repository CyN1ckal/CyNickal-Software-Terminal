// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "chart/CChartCommand.h"
#include "chart/CChartSettings.h"
#include "market_data/Store.h"
#include "market_data/Types.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace terminal {

enum class ChartLoadStatus : std::uint8_t
{
    Unconfigured,     // empty symbol
    Ready,            // bars non-empty
    Empty,            // instrument resolved, zero bars in the window
    UnknownSymbol,    // neither the stored FIGI nor the symbol resolves
    Unsupported,      // !isChartSettingsSupported
    Busy,             // caught Store exception classified by isStoreBusyError
    Error             // any other caught exception (including store-open failure at the pane)
};

struct ChartLoadResult
{
    ChartLoadStatus status{ChartLoadStatus::Unconfigured};
    std::string message;
    std::optional<Instrument> instrument;
    std::vector<Bar> bars;
    std::optional<SessionDate> first_session;
    std::optional<SessionDate> last_session;
    int sessions_used{};
    UnixSeconds ts_begin{};
    UnixSeconds ts_end{};
    // Newest coverage ingested_at among the sessions this load used.
    std::optional<UnixSeconds> received_at;
};

[[nodiscard]] std::string normalizeChartSymbol(std::string_view symbol);

// A stored FIGI wins and never falls back to the symbol, so a pinned book keeps its
// security after the ticker is reused. Otherwise Store::resolveSymbol: the open
// listing, else the instrument that most recently gave the symbol up.
[[nodiscard]] std::optional<Instrument> resolveChartInstrument(const Store& store,
                                                               std::string_view figi,
                                                               std::string_view symbol);

[[nodiscard]] ChartLoadResult loadChartBars(const Store& store, const CChartSettings& settings);
[[nodiscard]] bool isStoreBusyError(std::string_view what) noexcept;

// nullopt when the chart can already draw this symbol, the symbol is empty, or the
// resolved instrument no longer has an open listing. A renamed instrument downloads
// under its current ticker. Day1 needs stored daily bars. Intraday periods need
// 1-minute bars. `today` is the ingest window's end date.
[[nodiscard]] std::optional<ChartDownloadRequest> chartDownloadRequest(const Store& store,
                                                                       const CChartSettings& settings,
                                                                       SessionDate today);

}  // namespace terminal
