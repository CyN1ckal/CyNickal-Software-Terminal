// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "market_data/SqliteVersion.h"

TEST_CASE("sqlite amalgamation version")
{
    CHECK(terminal::sqliteLibVersionNumber() == 3053004);
}
