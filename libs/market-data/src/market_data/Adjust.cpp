// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "market_data/Adjust.h"

#include <vector>

namespace terminal {
namespace {

struct SplitFactor
{
    UnixSeconds ex_ts{};
    double ratio{};
};

}  // namespace

std::vector<Bar> adjustBarsForSplits(std::vector<Bar> bars, std::span<const CorporateAction> actions)
{
    std::vector<SplitFactor> splits;
    splits.reserve(actions.size());
    for (const CorporateAction& action : actions)
    {
        if (action.type != CorporateActionType::Split || !action.split_ratio.has_value())
        {
            continue;
        }
        const double ratio = *action.split_ratio;
        if (!(ratio > 0.0))
        {
            continue;
        }
        splits.push_back(SplitFactor{.ex_ts=action.ex_ts, .ratio=ratio});
    }
    if (splits.empty())
    {
        return bars;
    }

    for (Bar& bar : bars)
    {
        double factor = 1.0;
        for (const SplitFactor& split : splits)
        {
            if (split.ex_ts > bar.ts)
            {
                factor *= split.ratio;
            }
        }
        bar.open /= factor;
        bar.high /= factor;
        bar.low /= factor;
        bar.close /= factor;
        bar.volume *= factor;
    }
    return bars;
}

}  // namespace terminal
