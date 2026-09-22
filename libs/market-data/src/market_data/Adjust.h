// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "market_data/Types.h"

#include <span>
#include <vector>

namespace terminal {

// Backward split adjustment of an as-traded series. Dividends are ignored.
// For each split with ex_ts > bar.ts, OHLC is divided by split_ratio (new/old)
// and volume is multiplied by it. A split with ex_ts == bar.ts is the first
// post-split bar and is left unchanged. queryBars stays as-traded.
[[nodiscard]] std::vector<Bar> adjustBarsForSplits(std::vector<Bar> bars,
                                                   std::span<const CorporateAction> actions);

}  // namespace terminal
