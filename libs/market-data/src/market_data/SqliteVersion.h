// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

namespace terminal {

// Thin wrap of sqlite3_libversion_number so tests/GUI never include sqlite3.h.
[[nodiscard]] int sqliteLibVersionNumber();

}  // namespace terminal
