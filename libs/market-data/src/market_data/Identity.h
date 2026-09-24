// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "market_data/OpenFigi.h"
#include "market_data/Store.h"
#include "market_data/Types.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace terminal {

// A verification younger than this is trusted without a network call.
inline constexpr UnixSeconds kVerifyMaxAge = 24 * 60 * 60;
// When OpenFIGI cannot be reached, a ticker verified within this window keeps ingesting.
inline constexpr UnixSeconds kVerifyGrace = 7 * 24 * 60 * 60;

struct ResolvedInstrument
{
    InstrumentId id{};
    // Empty, or one line for the log or status line (a grace-window warning, a
    // rename found from the new ticker, a recycled ticker's former holder).
    std::string notice;
};

// Resolves the ticker MBoum is about to be asked for, per docs/composite-figi-identity.md.
// Throws with a message for the user when the symbol is refused; nothing is written then.
[[nodiscard]] ResolvedInstrument ensureInstrument(Store& store,
                                                  OpenFigiClient& figi,
                                                  std::string_view symbol,
                                                  UnixSeconds now);

enum class VerifyOutcome : std::uint8_t
{
    Ok,
    Renamed,
    Delisted,
    Conflict,
    Unreachable,  // the forward lookup itself learned nothing; the grace window applies
    Unresolved,   // forward contradicted the stored FIGI, then a follow-up lookup failed
    Skipped       // no FIGI, no open listing, or unknown id
};

struct VerifyReport
{
    InstrumentId id{};
    std::string symbol;        // the open symbol before verification
    VerifyOutcome outcome{VerifyOutcome::Skipped};
    std::string new_symbol;    // Renamed only
    std::string detail;
};

[[nodiscard]] std::string_view toString(VerifyOutcome outcome) noexcept;
// "ok", "renamed FB -> META", "delisted", "conflict: ...", "unreachable: ...".
[[nodiscard]] std::string describe(const VerifyReport& report);

// Forward-maps each open symbol. Anything but the stored FIGI reverse-maps the FIGI:
// its own symbol back means delisted; another ticker T is confirmed forward and
// becomes a rename. All listing changes land in one transaction, closes first.
// A failure after a conclusive forward mismatch is Unresolved: nothing changes and
// ensureInstrument refuses, because the stored ticker is known not to name the FIGI.
// One report per id, in order.
[[nodiscard]] std::vector<VerifyReport> verifyIdentities(Store& store,
                                                         OpenFigiClient& figi,
                                                         UnixSeconds now,
                                                         std::span<const InstrumentId> ids);

// Instruments with a FIGI and an open listing whose verified_at is NULL or at least
// kVerifyMaxAge old. all ignores the age.
[[nodiscard]] std::vector<InstrumentId> instrumentsDueForVerification(const Store& store,
                                                                      UnixSeconds now,
                                                                      bool all);

}  // namespace terminal
