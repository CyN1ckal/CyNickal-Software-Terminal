// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/studies/CVolume.h"

#include "chart/studies/StudyRegistry.h"

#include <algorithm>

namespace terminal {
namespace {

void labelVolume(std::span<const int> /*options*/, std::string& label)
{
    label = CVolume::label();
}

void processVolume(std::span<const Bar> bars,
                   std::span<const int> /*options*/,
                   std::string& label,
                   std::vector<StudyTrace>& traces)
{
    label = CVolume::label();
    traces.push_back(StudyTrace{.label = label, .values = CVolume::process(bars)});
}

constexpr StudyOutput kOutputs[] = {
    {.key = "up", .label = "Up", .palette_index = 2},
    {.key = "down", .label = "Down", .palette_index = 3},
};

constexpr StudyType kType{
    .id = "volume",
    .display_name = "Volume",
    .default_chart_region = 2,
    .graph = StudyGraph::Histogram,
    .anchor_zero = true,
    .color_by_bar = true,
    .value_decimals = 0,
    .note = "Each bar's volume. Up and down follow the candle.",
    .palette_index = 2,
    .outputs = kOutputs,
    .process = &processVolume,
    .label = &labelVolume,
};

struct Registration
{
    Registration() noexcept
    {
        registerStudy(kType);
    }
};

[[maybe_unused]] const Registration kVolumeRegistration{};

}  // namespace

std::vector<double> CVolume::process(std::span<const Bar> bars)
{
    std::vector<double> values(bars.size());
    std::ranges::transform(bars, values.begin(), &Bar::volume);
    return values;
}

std::string_view CVolume::label()
{
    return "Vol";
}

}  // namespace terminal
