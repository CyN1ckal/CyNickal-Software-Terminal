// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "ui/CommandMatch.h"

TEST_CASE("command palette matching")
{
    using terminal::commandMatchScore;

    CHECK(commandMatchScore("", "File Save") == 0);
    CHECK(commandMatchScore("   ", "File Save") == 0);
    CHECK(commandMatchScore("xyz", "File Save") < 0);
    CHECK(commandMatchScore("SAVE", "File Save") > 0);

    // Scattered letters still match, below whole words.
    CHECK(commandMatchScore("nfin", "View New Financials") > 0);
    CHECK(commandMatchScore("fin", "View New Financials") > commandMatchScore("nfin", "View New Financials"));

    // Scattered letters start at a word, not inside one.
    CHECK(commandMatchScore("new", "Settings Colors: Green up / Red down") < 0);

    // Every word must match; order between words does not matter.
    CHECK(commandMatchScore("fin new", "View New Financials") > 0);
    CHECK(commandMatchScore("fin chain", "View New Financials") < 0);

    // "save as" picks Save As over Save All.
    CHECK(commandMatchScore("save as", "File Save As...") > 0);
    CHECK(commandMatchScore("save as", "File Save All") < 0);

    // A word start beats the same letters inside a word.
    CHECK(commandMatchScore("chart", "Chart New Chart") > commandMatchScore("hart", "Chart New Chart"));
    CHECK(commandMatchScore("opt", "View New Options Chain") > commandMatchScore("opt", "View Close Portfolio"));

    // A letter that only appears in the last place the text can show it still matches.
    // Both the whole-word scan and the scattered scan stop one character short if their
    // end test is off by one.
    CHECK(commandMatchScore("x", "File Adx") > 0);
    CHECK(commandMatchScore("zx", "File Zqx") > 0);
    CHECK(commandMatchScore("x", "File Ad") < 0);
}
