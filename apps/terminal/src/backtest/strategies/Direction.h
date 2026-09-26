// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "chart/studies/StudyRegistry.h"

namespace terminal {

// The "direction" input every built-in strategy takes. Index 0 never goes short.
inline constexpr StudyChoice kDirectionChoices[] = {
    {.token = "long", .label = "Long only"},
    {.token = "long_short", .label = "Long and short"},
};

[[nodiscard]] constexpr bool directionAllowsShort(int choice) noexcept
{
    return choice == 1;
}

}  // namespace terminal
