// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "chart/studies/StudyOption.h"
#include "market_data/Types.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace terminal {

// Writes one target per bar, in bar order: the position the strategy wants after
// that bar closes, as a signed fraction of one sizing unit (1 long, -1 short, 0 flat).
// NaN is no opinion; the engine keeps the previous target. A target may depend only
// on bars up to and including its own, which strategy_tests checks for every entry.
// options is parallel to StrategyType::options and already clamped.
using StrategyProcess = void (*)(std::span<const Bar> bars, std::span<const int> options, std::vector<double>& targets);

// Filled by a strategy translation unit and passed to registerStrategy. The object,
// its option tables, and its callback must outlive the process. Inputs reuse the
// study option shape so the settings UI can draw them the same way.
struct StrategyType
{
    const char* id{""};
    const char* display_name{""};
    const char* note{nullptr};
    std::span<const StudyOption> options{};
    StrategyProcess process{nullptr};
};

// Strategies call this from their own static initialization. Duplicate ids keep the first.
void registerStrategy(const StrategyType& type) noexcept;

[[nodiscard]] const StrategyType* findStrategy(std::string_view id) noexcept;

[[nodiscard]] std::span<const StrategyType* const> strategyTypes() noexcept;

// Fallbacks for missing entries; integers clamped to [min, max]; choice indexes kept in range.
[[nodiscard]] std::vector<int> clampStrategyOptions(const StrategyType& type, std::span<const int> options);

// {"key": value, ...} in option order. A choice is written as its token.
[[nodiscard]] std::string strategyParamsJson(const StrategyType& type, std::span<const int> options);

// The inverse of strategyParamsJson. Unknown keys are ignored and missing ones fall back.
// nullopt when the text is not a JSON object or a choice token is unknown.
[[nodiscard]] std::optional<std::vector<int>> strategyOptionsFromJson(const StrategyType& type, std::string_view json);

// Runs the strategy on clamped options. Always returns bars.size() targets.
[[nodiscard]] std::vector<double> strategyTargets(const StrategyType& type,
                                                  std::span<const Bar> bars,
                                                  std::span<const int> options);

}  // namespace terminal
