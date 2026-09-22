// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "market_data/Types.h"

#include <span>
#include <string_view>
#include <vector>

namespace terminal {

// Copies each bar's volume. Up and down colors live on the study outputs.
class CVolume
{
public:
    [[nodiscard]] static std::vector<double> process(std::span<const Bar> bars);
    [[nodiscard]] static std::string_view label();
};

}  // namespace terminal
