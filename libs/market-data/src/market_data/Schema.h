// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include <string_view>

namespace terminal {

// schema/v4.sql, the baseline. v1..v3 were retired by the FIGI identity change.
[[nodiscard]] std::string_view schemaV4();

}  // namespace terminal
