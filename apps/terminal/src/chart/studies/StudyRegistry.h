// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "market_data/Types.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace terminal {

// One persisted input. An empty choices span is an integer in [min, max].
// Otherwise the stored value is an index into choices.
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
    int min{0};
    int max{0};
    int fallback{0};
    std::span<const StudyChoice> choices{};
    bool shown{true};
};

enum class StudyGraph : std::uint8_t
{
    Line = 0,
    Histogram,
};

// One drawn output. values lines up with the input bars. NaN is a gap.
// An empty bar span writes an empty values list.
struct StudyTrace
{
    std::string label;
    std::vector<double> values;
};

// One subgraph, in the same order process() appends traces.
// key is the chartbook token. label is the settings name.
// palette_index -1 uses the study's palette slot plus this output's position.
// A fixed palette_index pins that output's default color.
struct StudyOutput
{
    const char* key{""};
    const char* label{""};
    int palette_index{-1};
};

// Writes one trace per drawn series, in output order. A single line is one trace;
// bands are several. A color-by-bar histogram emits one trace; its second output
// is the down color, not another series.
// Region and graph come from the study. Color and line style are applied later.
// label is the instance name (toolbar), not one of the traces.
// options is parallel to StudyType::options.
using StudyProcess = void (*)(std::span<const Bar> bars,
                              std::span<const int> options,
                              std::string& label,
                              std::vector<StudyTrace>& traces);

// Instance name only. Does not walk the bars.
using StudyLabel = void (*)(std::span<const int> options, std::string& label);

// Filled by a study translation unit and passed to registerStudy.
// The pointed-to object, its option tables, and its callback must outlive the process.
struct StudyType
{
    const char* id{""};
    const char* display_name{""};
    int default_chart_region{1};
    StudyGraph graph{StudyGraph::Line};
    bool anchor_zero{false};
    // Histogram uses output 0 when the candle closes up and output 1 when it closes down.
    // process() still emits one trace.
    bool color_by_bar{false};
    int value_decimals{4};
    const char* note{nullptr};
    // -1 cycles the chart palette. Any other value is a fixed palette slot.
    int palette_index{-1};
    std::span<const StudyOption> options{};
    std::span<const StudyOutput> outputs{};
    StudyProcess process{nullptr};
    StudyLabel label{nullptr};
};

// Studies call this from their own static initialization. Duplicate ids keep the first.
void registerStudy(const StudyType& type) noexcept;

[[nodiscard]] const StudyType* findStudy(std::string_view id) noexcept;

[[nodiscard]] std::span<const StudyType* const> studyTypes() noexcept;

}  // namespace terminal
