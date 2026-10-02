// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include <string_view>

namespace terminal {

// Ranks a command for the palette. Higher is better; negative means no match.
// Each space-separated word of the query must appear in the text, case-insensitively,
// either whole or as characters in order starting at a word. Whole words beat scattered letters, and a
// match at the start of a word beats one inside it. An empty query matches with 0.
[[nodiscard]] int commandMatchScore(std::string_view query, std::string_view text) noexcept;

}  // namespace terminal
