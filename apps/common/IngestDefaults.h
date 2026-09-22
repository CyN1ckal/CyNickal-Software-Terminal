// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

namespace terminal {

// One FROM preset for the ingest CLI, DATA, and the chart's daily floor.
inline constexpr int kIngestDefaultIntradayDays = 14;
inline constexpr int kIngestDefaultDailyDays = 365 * 5;

}  // namespace terminal
