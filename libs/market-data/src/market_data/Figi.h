// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace terminal {

// OMG FIGI shape: 12 characters. Positions 1-2 are consonants and not BS, BM, GG,
// GB, GH, KY, or VG. Position 3 is G. Positions 4-11 are consonants or digits.
// Position 12 is the check digit. Uppercase only. schema/v4.sql checks the same shape.
[[nodiscard]] bool isFigiShape(std::string_view figi) noexcept;

// Check digit for the first 11 characters: A=10..Z=35, double every even (1-based)
// position, sum the decimal digits, (10 - sum % 10) % 10. nullopt when a
// character is not a digit or an uppercase letter.
[[nodiscard]] std::optional<char> figiCheckDigit(std::string_view first11) noexcept;

// isFigiShape and the check digit. Every write path calls this; reads do not.
[[nodiscard]] bool isValidFigi(std::string_view figi) noexcept;

// A valid FIGI derived from seed ("ZZG" + 8 characters + check digit). Tests only:
// the same seed gives the same FIGI.
[[nodiscard]] std::string testingFigiFor(std::string_view seed);

}  // namespace terminal
