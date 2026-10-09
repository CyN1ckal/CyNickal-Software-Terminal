// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include <span>

namespace terminal {

// One persisted input. An empty choices span is an integer in [min, max].
// Otherwise the stored value is an index into choices.
// Studies and backtest strategies share this shape so one settings row can draw both.
struct StudyChoice
{
    const char* token{""};
    const char* label{""};
    // Selecting this choice on the price graph moves the study to the pane below it.
    bool leave_price_scale{false};
};

struct StudyOption
{
    const char* key{""};
    const char* label{""};
    std::span<const StudyChoice> choices{};
    int min{0};
    int max{0};
    int fallback{0};
    bool shown{true};
};

}  // namespace terminal
