// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "market_data/Types.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace terminal {

class IngestWorker;
class Store;

// One underlying's option slice: the symbol the user typed, the expiration they
// chose, and the quotes the store holds for it. It reads the store each time the
// selection changes and asks the ingest worker for a slice that is missing.
// Every window that shows a chain owns one of these.
class OptionChainSource
{
public:
    // Restores a saved selection. expiration_type is "weekly" or "monthly"; any
    // other value, or expiration 0, leaves the expiration to the newest fetched slice.
    void restore(std::string_view symbol, std::string_view figi, SessionDate expiration,
                 std::string_view expiration_type);

    // SYMBOL, EXPIRATION, and GO on one line. True when the user switched to a
    // different underlying, so anything priced on the old one is stale.
    bool drawPicker(IngestWorker* ingest);

    // Inbound symbol from a link group. Same text returns without clearing the chain.
    // Does not publish. The caller mirrors this into the edit buffer.
    void applyLinkedSymbol(std::string_view symbol);

    // Call once per frame after drawPicker.
    void refresh(Store* store, IngestWorker* ingest);

    [[nodiscard]] const std::string& symbol() const noexcept;
    [[nodiscard]] const std::string& figi() const noexcept;
    [[nodiscard]] bool hasExpiration() const noexcept;
    [[nodiscard]] SessionDate expiration() const noexcept;
    [[nodiscard]] OptionExpirationType expirationType() const noexcept;
    [[nodiscard]] const OptionExpiry* selectedExpiry() const;
    [[nodiscard]] std::span<const OptionQuote> quotes() const noexcept;
    [[nodiscard]] const std::optional<OptionUnderlying>& underlying() const noexcept;
    [[nodiscard]] const std::string& status() const noexcept;
    [[nodiscard]] bool failed() const noexcept;
    [[nodiscard]] bool fetching() const;

    // "2026-10-16 weekly", or empty without an expiration.
    [[nodiscard]] std::string expirationLabel() const;

private:
    void requestFetch(IngestWorker* ingest, bool force);
    [[nodiscard]] std::string viewKey() const;

    char symbol_input_[32]{};
    std::string active_symbol_;
    std::string active_figi_;  // pinned once the symbol resolves; cleared when a new symbol is typed
    SessionDate expiration_{0};
    OptionExpirationType expiration_type_{OptionExpirationType::Weekly};
    bool has_expiration_{false};
    std::vector<OptionExpiry> expiries_;
    std::vector<OptionQuote> quotes_;
    std::optional<OptionUnderlying> underlying_;
    std::string loaded_key_;
    std::string failed_key_;
    std::string inflight_key_;
    std::string status_{"enter a symbol"};
    std::string error_;
    std::uint64_t inflight_serial_{0};
    bool inflight_{false};
    bool have_slice_{false};
    bool busy_{false};
    bool blocked_{false};
    bool fetch_now_{false};
    bool needs_reload_{true};
};

}  // namespace terminal
