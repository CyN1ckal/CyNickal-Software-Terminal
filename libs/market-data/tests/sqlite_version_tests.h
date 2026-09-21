#pragma once

#include "market_data/SqliteVersion.h"

TEST_CASE("sqlite amalgamation version")
{
    CHECK(terminal::sqliteLibVersionNumber() == 3053004);
}
