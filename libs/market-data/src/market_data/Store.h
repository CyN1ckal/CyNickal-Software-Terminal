// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "market_data/Types.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace terminal {

enum class StoreMode : std::uint8_t
{
    Writer,
    Reader
};

class Store
{
public:
    explicit Store(std::filesystem::path db_path, StoreMode mode = StoreMode::Writer);
    ~Store();

    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;
    Store(Store&&) = delete;
    Store& operator=(Store&&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept;
    [[nodiscard]] int userVersion() const;
    [[nodiscard]] std::vector<std::string> tableNames() const;
    [[nodiscard]] bool foreignKeysEnabled() const;

    static void testingSetUserVersion(const std::filesystem::path& path, int version);

    InstrumentId upsertInstrument(const Instrument& instrument);
    [[nodiscard]] std::optional<Instrument> findInstrument(
        std::string_view symbol,
        std::optional<std::string_view> exchange = std::nullopt) const;
    [[nodiscard]] std::vector<Instrument> findInstrumentsBySymbol(std::string_view symbol) const;
    [[nodiscard]] std::optional<Instrument> findInstrumentById(InstrumentId id) const;

    UpsertBarsResult upsertBars(std::span<const Bar> bars);
    [[nodiscard]] std::vector<Bar> queryBars(InstrumentId id,
                                             int timeframe_s,
                                             UnixSeconds ts_begin,
                                             UnixSeconds ts_end) const;

    void upsertCoverage(const CoverageDay& row);
    [[nodiscard]] std::vector<CoverageDay> queryIncompleteCoverage(InstrumentId id,
                                                                   int timeframe_s) const;
    [[nodiscard]] std::vector<CoverageDay> queryCoverageDays(InstrumentId id, int timeframe_s) const;
    [[nodiscard]] std::vector<CoverageSummary> queryCoverageSummaries(int timeframe_s) const;
    [[nodiscard]] std::optional<CoverageDay> findCoverage(InstrumentId id,
                                                          int timeframe_s,
                                                          SessionDate session_date) const;

    CoverageDay refreshCoverageFromBars(InstrumentId id,
                                        int timeframe_s,
                                        SessionDate session_date,
                                        std::optional<int> expected_count = std::nullopt,
                                        bool session_still_open = false);

    IngestSessionResult ingestSession(std::span<const Bar> bars,
                                      InstrumentId id,
                                      int timeframe_s,
                                      SessionDate session_date,
                                      std::optional<int> expected_count = std::nullopt,
                                      bool session_still_open = false);

    IngestDailyRangeResult ingestDailyRange(std::span<const Bar> bars,
                                            InstrumentId id,
                                            SessionDate from,
                                            SessionDate to);

    void upsertCorporateAction(const CorporateAction& action);
    [[nodiscard]] std::vector<CorporateAction> queryCorporateActions(InstrumentId id,
                                                                     UnixSeconds from_ex_ts,
                                                                     UnixSeconds to_ex_ts) const;

private:
    UpsertBarsResult upsertBarsUnlocked(std::span<const Bar> bars,
                                        UnixSeconds now,
                                        const Instrument* session_filter,
                                        int timeframe_s,
                                        SessionDate session_date,
                                        std::optional<int> expected_count,
                                        std::optional<SessionDate> session_date_end = std::nullopt);
    CoverageDay refreshCoverageFromBarsUnlocked(InstrumentId id,
                                                int timeframe_s,
                                                SessionDate session_date,
                                                std::optional<int> expected_count,
                                                bool session_still_open);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace terminal
