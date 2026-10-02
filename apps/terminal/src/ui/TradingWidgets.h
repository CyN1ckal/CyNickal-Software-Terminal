// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "market_data/Store.h"

#include "imgui.h"

#include <exception>
#include <string>
#include <utility>

namespace terminal {

// ImGui pieces shared by the ledger, statistics, and backtest panels.

// Right-aligned in the current cell, in the mono face.
void drawRightText(const std::string& text, const ImVec4& color);

// Money right-aligned. Negative amounts are kDown; signed also paints positive amounts kUp.
void drawMoneyCell(double amount, bool signed_color = false);

// GO-style primary button.
[[nodiscard]] bool primaryButton(const char* label);

// The GUI Store is a reader. Writes open a short-lived writer on the same file.
// error holds the exception text on failure and is cleared on success.
template <typename Fn>
bool withWriter(const Store* reader, std::string& error, Fn&& action)
{
    if (reader == nullptr)
    {
        error = "market data is unavailable";
        return false;
    }
    try
    {
        Store writer(reader->path(), StoreMode::Writer);
        std::forward<Fn>(action)(writer);
        error.clear();
        return true;
    }
    catch (const std::exception& ex)
    {
        error = ex.what();
        return false;
    }
}

}  // namespace terminal
