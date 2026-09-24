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

// One step of Store::applyListingChanges.
struct ListingChange
{
    enum class Kind : std::uint8_t
    {
        Close,          // close the open listing on instrument_id with reason
        Open,           // open symbol on instrument_id
        MarkVerified,   // verified_at = now
        ClearVerified   // verified_at = NULL
    };

    Kind kind{Kind::MarkVerified};
    InstrumentId instrument_id{};
    ListingCloseReason reason{ListingCloseReason::Manual};
    std::string symbol;
};

// Listing symbols are stored uppercase with "." as the share-class separator, the spelling
// MBoum answers to ("meta" -> "META", "$spx" -> "$SPX", "brk/b" -> "BRK.B").
[[nodiscard]] std::string canonicalListingSymbol(std::string_view symbol);

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
    [[nodiscard]] std::vector<std::string> viewNames() const;
    [[nodiscard]] bool foreignKeysEnabled() const;

    static void testingSetUserVersion(const std::filesystem::path& path, int version);
    // Reads the file on its own connection, without migrating. For tests.
    [[nodiscard]] static int testingUserVersion(const std::filesystem::path& path);
    [[nodiscard]] static std::vector<std::string> testingTableNames(const std::filesystem::path& path);
    // Applies schemaV4() to a user_version 0 file and stamps 4. Does not run schemaV5().
    static void testingCreateSchemaV4(const std::filesystem::path& path);
    // One equity (testingFigiFor(symbol)) and an open listing on a version-4 file
    // that has no portfolio table. Leaves user_version at 4.
    static void testingSeedV4Instrument(const std::filesystem::path& path, std::string_view symbol);
    // Inserts an equity (or asset_class) row with testingFigiFor(symbol), an open
    // listing, and verified_at = now. For tests that need an instrument and do not
    // care about its FIGI.
    InstrumentId testingInsertInstrument(std::string_view symbol,
                                         AssetClass asset_class = AssetClass::Equity);

    // Inserts the row and an open listing on instrument.symbol (uppercased) in one transaction.
    // figi must pass isValidFigi when set and is required for equity, etf, and index.
    // Throws when the FIGI is already stored or the symbol already has an open listing.
    // now 0 means the current time; it becomes created_at and opened_at.
    InstrumentId insertInstrument(const Instrument& instrument, UnixSeconds now = 0);
    [[nodiscard]] std::optional<Instrument> findInstrumentById(InstrumentId id) const;
    [[nodiscard]] std::optional<Instrument> findInstrumentByFigi(std::string_view figi) const;
    // The instrument whose listing for symbol is open. Ingest uses this.
    [[nodiscard]] std::optional<Instrument> findOpenListing(std::string_view symbol) const;
    // The open listing when there is one, otherwise the instrument whose listing for
    // symbol closed most recently. Charts and panels use this.
    [[nodiscard]] std::optional<Instrument> resolveSymbol(std::string_view symbol) const;
    // The most recently closed listing for symbol on any instrument.
    [[nodiscard]] std::optional<InstrumentListing> latestClosedListing(std::string_view symbol) const;
    // Every listing of one instrument, oldest first.
    [[nodiscard]] std::vector<InstrumentListing> listingHistory(InstrumentId id) const;
    // Every instrument, ordered by id.
    [[nodiscard]] std::vector<Instrument> listInstruments() const;

    // Requires no open listing on id and none on symbol. Clears delisted_at.
    void openListing(InstrumentId id, std::string_view symbol, UnixSeconds now);
    // Closes the open listing on id. Delisted also sets delisted_at when it is NULL.
    void closeListing(InstrumentId id, UnixSeconds now, ListingCloseReason reason);
    // closeListing(renamed) then openListing, in one transaction.
    void relinkSymbol(InstrumentId id, std::string_view symbol, UnixSeconds now);
    // Fills a NULL FIGI (future, crypto, other). The same value is a no-op.
    // A different value, or one stored on another id, throws.
    void attachFigi(InstrumentId id, std::string_view figi);
    void markVerified(InstrumentId id, UnixSeconds now);
    void clearVerified(InstrumentId id);
    // name, asset_class, currency, and timezone. A timezone change after bars exist throws.
    void updateDescriptive(InstrumentId id, const Instrument& fields);
    // Applies the changes in order in one transaction. An Open whose instrument or
    // symbol already has an open listing is skipped, clears that instrument's
    // verified_at, and its index is returned.
    std::vector<std::size_t> applyListingChanges(std::span<const ListingChange> changes, UnixSeconds now);

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

    // Replaces one statement grid. Cells omitted from the span are deleted.
    // An empty span still records the snapshot (a fetch that returned no facts).
    void replaceStatement(const StatementSnapshot& snapshot, std::span<const StatementCell> cells);
    [[nodiscard]] std::optional<StatementSnapshot> findStatementSnapshot(
        InstrumentId id,
        StatementKind statement,
        StatementTimeframe timeframe) const;
    [[nodiscard]] std::vector<StatementCell> queryStatementCells(
        InstrumentId id,
        StatementKind statement,
        StatementTimeframe timeframe) const;
    [[nodiscard]] std::vector<StatementCell> queryStatementLine(InstrumentId id,
                                                                StatementKind statement,
                                                                StatementTimeframe timeframe,
                                                                std::string_view line_item) const;
    [[nodiscard]] std::vector<StatementCell> queryStatementPeriod(InstrumentId id,
                                                                  StatementKind statement,
                                                                  StatementTimeframe timeframe,
                                                                  std::string_view period_end) const;

    // Replaces the slices in batches and, when replace_calendar is set, the
    // expiration calendar. Slices omitted from a new calendar are deleted.
    void replaceOptionChain(const OptionChainWrite& write);
    [[nodiscard]] std::optional<OptionUnderlying> findOptionUnderlying(InstrumentId id) const;
    [[nodiscard]] std::vector<OptionExpiry> queryOptionExpiries(InstrumentId id) const;
    [[nodiscard]] std::vector<OptionQuote> queryOptionQuotes(InstrumentId id,
                                                             SessionDate expiration,
                                                             OptionExpirationType expiration_type) const;

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
