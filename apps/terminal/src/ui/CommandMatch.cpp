// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "ui/CommandMatch.h"

#include <algorithm>
#include <cctype>
#include <cstddef>

namespace terminal {
namespace {

constexpr int kWordStartSubstring = 300;
constexpr int kInnerSubstring = 200;
constexpr int kScattered = 100;
constexpr int kScatteredWordStart = 10;
constexpr int kScatteredAdjacent = 5;

[[nodiscard]] char lower(char c) noexcept
{
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

[[nodiscard]] bool isWordStart(std::string_view text, std::size_t at) noexcept
{
    if (at == 0)
    {
        return true;
    }
    const auto prev = static_cast<unsigned char>(text[at - 1]);
    const auto here = static_cast<unsigned char>(text[at]);
    return std::isalnum(prev) == 0 || (std::islower(prev) != 0 && std::isupper(here) != 0);
}

[[nodiscard]] std::size_t findInsensitive(std::string_view text, std::string_view word, std::size_t from) noexcept
{
    if (word.size() > text.size())
    {
        return std::string_view::npos;
    }
    for (std::size_t at = from; at + word.size() <= text.size(); ++at)
    {
        bool same = true;
        for (std::size_t i = 0; i < word.size(); ++i)
        {
            if (lower(text[at + i]) != lower(word[i]))
            {
                same = false;
                break;
            }
        }
        if (same)
        {
            return at;
        }
    }
    return std::string_view::npos;
}

[[nodiscard]] int scoreWord(std::string_view word, std::string_view text) noexcept
{
    // Whole word: prefer an occurrence that starts a word, then the earliest.
    int best = -1;
    for (std::size_t at = findInsensitive(text, word, 0); at != std::string_view::npos;
         at = findInsensitive(text, word, at + 1))
    {
        const int base = isWordStart(text, at) ? kWordStartSubstring : kInnerSubstring;
        best = std::max(best, base - static_cast<int>(at));
    }
    if (best >= 0)
    {
        return best;
    }

    // Scattered letters in order.
    int score = kScattered;
    std::size_t at = 0;
    std::size_t first = std::string_view::npos;
    std::size_t last = 0;
    for (std::size_t i = 0; i < word.size(); ++i)
    {
        const char want = lower(word[i]);
        while (at < text.size() && lower(text[at]) != want)
        {
            ++at;
        }
        if (at >= text.size())
        {
            return -1;
        }
        // Scattered letters must begin a word, or "new" would match "greeN up / rEd doWn".
        if (i == 0 && !isWordStart(text, at))
        {
            while (at < text.size() && (lower(text[at]) != want || !isWordStart(text, at)))
            {
                ++at;
            }
            if (at >= text.size())
            {
                return -1;
            }
        }
        if (isWordStart(text, at))
        {
            score += kScatteredWordStart;
        }
        if (i > 0 && at == last + 1)
        {
            score += kScatteredAdjacent;
        }
        if (first == std::string_view::npos)
        {
            first = at;
        }
        last = at;
        ++at;
    }
    return score - static_cast<int>(last - first);
}

}  // namespace

int commandMatchScore(std::string_view query, std::string_view text) noexcept
{
    int total = 0;
    std::size_t at = 0;
    while (at < query.size())
    {
        while (at < query.size() && query[at] == ' ')
        {
            ++at;
        }
        const std::size_t end = std::min(query.find(' ', at), query.size());
        if (end > at)
        {
            const int word = scoreWord(query.substr(at, end - at), text);
            if (word < 0)
            {
                return -1;
            }
            total += word;
        }
        at = end;
    }
    return total;
}

}  // namespace terminal
