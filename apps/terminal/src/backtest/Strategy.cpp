// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "backtest/Strategy.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace terminal {
namespace {

struct Registry
{
    static constexpr int kCapacity = 32;
    const StrategyType* types[kCapacity]{};
    int count{0};
};

Registry& registry() noexcept
{
    static Registry types;
    return types;
}

[[nodiscard]] int clampOne(const StudyOption& option, int value) noexcept
{
    if (!option.choices.empty())
    {
        const int last = static_cast<int>(option.choices.size()) - 1;
        return value < 0 || value > last ? std::clamp(option.fallback, 0, last) : value;
    }
    return std::clamp(value, option.min, option.max);
}

}  // namespace

void registerStrategy(const StrategyType& type) noexcept
{
    Registry& types = registry();
    if (type.id == nullptr || type.process == nullptr || types.count >= Registry::kCapacity)
    {
        return;
    }
    for (int index = 0; index < types.count; ++index)
    {
        const StrategyType* existing = types.types[static_cast<std::size_t>(index)];
        if (existing != nullptr && existing->id != nullptr && type.id == std::string_view{existing->id})
        {
            return;
        }
    }
    types.types[static_cast<std::size_t>(types.count)] = &type;
    ++types.count;
}

const StrategyType* findStrategy(std::string_view id) noexcept
{
    const Registry& types = registry();
    for (int index = 0; index < types.count; ++index)
    {
        const StrategyType* type = types.types[static_cast<std::size_t>(index)];
        if (type != nullptr && type->id != nullptr && id == type->id)
        {
            return type;
        }
    }
    return nullptr;
}

std::span<const StrategyType* const> strategyTypes() noexcept
{
    const Registry& types = registry();
    return {types.types, static_cast<std::size_t>(types.count)};
}

std::vector<int> clampStrategyOptions(const StrategyType& type, std::span<const int> options)
{
    std::vector<int> clamped;
    clamped.reserve(type.options.size());
    for (std::size_t index = 0; index < type.options.size(); ++index)
    {
        const StudyOption& option = type.options[index];
        const int value = index < options.size() ? options[index] : option.fallback;
        clamped.push_back(clampOne(option, value));
    }
    return clamped;
}

std::string strategyParamsJson(const StrategyType& type, std::span<const int> options)
{
    const std::vector<int> clamped = clampStrategyOptions(type, options);
    nlohmann::ordered_json object = nlohmann::ordered_json::object();
    for (std::size_t index = 0; index < type.options.size(); ++index)
    {
        const StudyOption& option = type.options[index];
        const int value = clamped[index];
        if (option.choices.empty())
        {
            object[option.key] = value;
        }
        else
        {
            object[option.key] = option.choices[static_cast<std::size_t>(value)].token;
        }
    }
    return object.dump();
}

std::optional<std::vector<int>> strategyOptionsFromJson(const StrategyType& type, std::string_view json)
{
    const nlohmann::json parsed = nlohmann::json::parse(json, nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object())
    {
        return std::nullopt;
    }
    std::vector<int> values;
    values.reserve(type.options.size());
    for (const StudyOption& option : type.options)
    {
        int value = option.fallback;
        const auto found = parsed.find(option.key);
        if (found != parsed.end())
        {
            if (option.choices.empty())
            {
                if (!found->is_number_integer())
                {
                    return std::nullopt;
                }
                const auto wide = found->get<std::int64_t>();
                value = static_cast<int>(std::clamp<std::int64_t>(wide, std::numeric_limits<int>::min(),
                                                                  std::numeric_limits<int>::max()));
            }
            else
            {
                if (!found->is_string())
                {
                    return std::nullopt;
                }
                const std::string token = found->get<std::string>();
                const auto choice = std::ranges::find_if(option.choices, [&](const StudyChoice& candidate) {
                    return token == candidate.token;
                });
                if (choice == option.choices.end())
                {
                    return std::nullopt;
                }
                value = static_cast<int>(choice - option.choices.begin());
            }
        }
        values.push_back(value);
    }
    return clampStrategyOptions(type, values);
}

std::vector<double> strategyTargets(const StrategyType& type, std::span<const Bar> bars, std::span<const int> options)
{
    const std::vector<int> clamped = clampStrategyOptions(type, options);
    std::vector<double> targets;
    targets.reserve(bars.size());
    if (type.process != nullptr)
    {
        type.process(bars, clamped, targets);
    }
    targets.resize(bars.size(), std::numeric_limits<double>::quiet_NaN());
    return targets;
}

}  // namespace terminal
