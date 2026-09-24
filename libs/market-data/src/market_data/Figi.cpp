// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "market_data/Figi.h"

#include <array>
#include <cstdint>

namespace terminal {
namespace {

constexpr std::size_t kFigiLength = 12;

[[nodiscard]] bool isVowel(char ch) noexcept
{
    return ch == 'A' || ch == 'E' || ch == 'I' || ch == 'O' || ch == 'U';
}

[[nodiscard]] bool isConsonant(char ch) noexcept
{
    return ch >= 'A' && ch <= 'Z' && !isVowel(ch);
}

[[nodiscard]] bool isDigit(char ch) noexcept
{
    return ch >= '0' && ch <= '9';
}

}  // namespace

bool isFigiShape(std::string_view figi) noexcept
{
    if (figi.size() != kFigiLength)
    {
        return false;
    }
    if (!isConsonant(figi[0]) || !isConsonant(figi[1]) || figi[2] != 'G')
    {
        return false;
    }
    constexpr std::array<std::string_view, 7> kReserved = {"BS", "BM", "GG", "GB", "GH", "KY", "VG"};
    for (const std::string_view prefix : kReserved)
    {
        if (figi.substr(0, 2) == prefix)
        {
            return false;
        }
    }
    for (std::size_t i = 3; i < kFigiLength - 1; ++i)
    {
        if (!isConsonant(figi[i]) && !isDigit(figi[i]))
        {
            return false;
        }
    }
    return isDigit(figi[kFigiLength - 1]);
}

std::optional<char> figiCheckDigit(std::string_view first11) noexcept
{
    if (first11.size() != kFigiLength - 1)
    {
        return std::nullopt;
    }
    int sum = 0;
    for (std::size_t i = 0; i < first11.size(); ++i)
    {
        const char ch = first11[i];
        int value = 0;
        if (isDigit(ch))
        {
            value = ch - '0';
        }
        else if (ch >= 'A' && ch <= 'Z')
        {
            value = ch - 'A' + 10;
        }
        else
        {
            return std::nullopt;
        }
        if (i % 2 == 1)
        {
            value *= 2;
        }
        while (value > 0)
        {
            sum += value % 10;
            value /= 10;
        }
    }
    return static_cast<char>('0' + ((10 - (sum % 10)) % 10));
}

bool isValidFigi(std::string_view figi) noexcept
{
    if (!isFigiShape(figi))
    {
        return false;
    }
    const auto digit = figiCheckDigit(figi.substr(0, kFigiLength - 1));
    return digit.has_value() && *digit == figi[kFigiLength - 1];
}

std::string testingFigiFor(std::string_view seed)
{
    // Consonants and digits only, so the body always passes isFigiShape.
    constexpr std::string_view kAlphabet = "0123456789BCDFGHJKLMNPQRSTVWXYZ";
    std::uint64_t hash = 1469598103934665603ULL;
    for (const char ch : seed)
    {
        hash ^= static_cast<unsigned char>(ch);
        hash *= 1099511628211ULL;
    }
    // Live FIGIs start BBG. ZZG keeps a test value from being mistaken for one.
    std::string figi = "ZZG";
    for (int i = 0; i < 8; ++i)
    {
        figi.push_back(kAlphabet[hash % kAlphabet.size()]);
        hash /= kAlphabet.size();
    }
    figi.push_back(figiCheckDigit(figi).value_or('0'));  // the body is always 11 valid characters
    return figi;
}

}  // namespace terminal
