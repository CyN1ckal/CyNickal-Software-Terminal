// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "market_data/SqliteVersion.h"

#include "sqlite3.h"

namespace terminal {

int sqliteLibVersionNumber()
{
    return sqlite3_libversion_number();
}

}  // namespace terminal
