// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/CStudyCompute.h"

#include "chart/studies/StudyRegistry.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>

namespace terminal {

bool isStudyInstanceSupported(const CStudyInstance& inst) noexcept
{
    const StudyType* type = findStudy(inst.type_id);
    return type != nullptr && type->process != nullptr && inst.options.size() == type->options.size();
}

void clampStudyOptions(CStudyInstance& inst) noexcept
{
    const StudyType* type = findStudy(inst.type_id);
    if (type == nullptr || inst.options.size() != type->options.size())
    {
        return;
    }
    for (std::size_t index = 0; index < type->options.size(); ++index)
    {
        const StudyOption& option = type->options[index];
        int& value = inst.options[index];
        if (!option.choices.empty())
        {
            const auto count = static_cast<int>(option.choices.size());
            if (value < 0 || value >= count)
            {
                value = option.fallback;
                if (value < 0 || value >= count)
                {
                    value = 0;
                }
            }
            continue;
        }
        value = std::clamp(value, option.min, option.max);
    }
}

namespace {

[[nodiscard]] CStudyOutputStyle studyOutputStyle(const CStudyInstance& inst, std::size_t index) noexcept
{
    if (index < inst.outputs.size())
    {
        return inst.outputs[index];
    }
    return CStudyOutputStyle{.color = inst.color, .line = StudyLineStyle::Solid};
}

}  // namespace

void normalizeStudyOutputs(CStudyInstance& inst)
{
    const StudyType* type = findStudy(inst.type_id);
    if (type == nullptr)
    {
        return;
    }
    // An empty directional study has no saved bar colors. Use the candle palette
    // instead of copying the old label color onto both bars.
    if (type->color_by_bar && inst.outputs.empty())
    {
        assignStudyOutputDefaults(inst, 0);
        return;
    }
    if (inst.outputs.size() == type->outputs.size())
    {
        if (!inst.outputs.empty())
        {
            inst.color = inst.outputs.front().color;
        }
        return;
    }
    std::vector<CStudyOutputStyle> next;
    next.reserve(type->outputs.size());
    for (std::size_t index = 0; index < type->outputs.size(); ++index)
    {
        if (index < inst.outputs.size())
        {
            next.push_back(inst.outputs[index]);
            continue;
        }
        next.push_back(CStudyOutputStyle{.color = inst.color, .line = StudyLineStyle::Solid});
    }
    inst.outputs = std::move(next);
    if (!inst.outputs.empty())
    {
        inst.color = inst.outputs.front().color;
    }
}

void assignStudyOutputDefaults(CStudyInstance& inst, int slot)
{
    const StudyType* type = findStudy(inst.type_id);
    if (type == nullptr)
    {
        return;
    }
    const int base = type->palette_index >= 0 ? type->palette_index : slot;
    inst.color = studyPaletteColor(base);
    inst.outputs.clear();
    inst.outputs.reserve(type->outputs.size());
    for (std::size_t index = 0; index < type->outputs.size(); ++index)
    {
        const StudyOutput& output = type->outputs[index];
        const int chosen =
            output.palette_index >= 0 ? output.palette_index : base + static_cast<int>(index);
        inst.outputs.push_back(
            CStudyOutputStyle{.color = studyPaletteColor(chosen), .line = StudyLineStyle::Solid});
    }
    if (!inst.outputs.empty())
    {
        inst.color = inst.outputs.front().color;
    }
}

std::string studyShortLabel(const CStudyInstance& inst)
{
    const StudyType* type = findStudy(inst.type_id);
    if (type == nullptr || type->label == nullptr || inst.options.size() != type->options.size())
    {
        return {};
    }
    std::string label;
    type->label(inst.options, label);
    return label;
}

std::vector<CStudySeries> computeStudies(std::span<const Bar> bars,
                                         std::span<const CStudyInstance> studies)
{
    std::vector<CStudySeries> out;
    for (const CStudyInstance& inst : studies)
    {
        if (!inst.enabled || !isStudyInstanceSupported(inst))
        {
            continue;
        }
        const StudyType* type = findStudy(inst.type_id);
        if (type == nullptr || type->process == nullptr)
        {
            continue;
        }
        std::string instance_label;
        std::vector<StudyTrace> traces;
        type->process(bars, inst.options, instance_label, traces);
        for (std::size_t index = 0; index < traces.size(); ++index)
        {
            StudyTrace& trace = traces[index];
            const auto style = studyOutputStyle(inst, index);
            CStudySeries series;
            series.study_id = inst.id;
            series.type_id = inst.type_id;
            series.chart_region = clampStudyChartRegion(inst.chart_region);
            series.placement = series.chart_region == kStudyMainChartRegion ? StudyPlacement::Overlay
                                                                            : StudyPlacement::Subgraph;
            series.color = style.color;
            series.line = style.line;
            series.histogram = type->graph == StudyGraph::Histogram;
            if (type->color_by_bar && index == 0)
            {
                series.color_by_bar = true;
                series.color = studyOutputStyle(inst, 0).color;
                series.down_color = studyOutputStyle(inst, 1).color;
            }
            series.anchor_zero = type->anchor_zero;
            series.value_decimals = type->value_decimals;
            series.label = trace.label.empty() ? instance_label : std::move(trace.label);
            series.values = std::move(trace.values);
            out.push_back(std::move(series));
        }
    }
    return out;
}

std::vector<CStudySeries> studiesForLoad(const ChartLoadResult& loaded,
                                         std::span<const CStudyInstance> studies)
{
    if (loaded.status == ChartLoadStatus::Ready && !loaded.bars.empty())
    {
        return computeStudies(loaded.bars, studies);
    }
    return {};
}

OverlayYExtent overlayYExtent(std::span<const CStudySeries> series,
                              const ChartVisibleWindow& win,
                              int bar_count) noexcept
{
    OverlayYExtent out;
    if (bar_count <= 0)
    {
        return out;
    }
    const int first = std::clamp(win.first, 0, bar_count - 1);
    const int last = std::clamp(win.last, 0, bar_count - 1);
    if (first > last)
    {
        return out;
    }
    for (const CStudySeries& item : series)
    {
        if (item.placement != StudyPlacement::Overlay ||
            item.values.size() != static_cast<std::size_t>(bar_count))
        {
            continue;
        }
        for (int i = first; i <= last; ++i)
        {
            const double value = item.values[static_cast<std::size_t>(i)];
            if (!std::isfinite(value))
            {
                continue;
            }
            if (!out.valid)
            {
                out.valid = true;
                out.min = value;
                out.max = value;
            }
            else
            {
                out.min = std::min(out.min, value);
                out.max = std::max(out.max, value);
            }
        }
    }
    return out;
}

int studyChartRegionCount(std::span<const CStudySeries> series) noexcept
{
    int max_region = kStudyMainChartRegion;
    for (const CStudySeries& item : series)
    {
        max_region = std::max(max_region, clampStudyChartRegion(item.chart_region));
    }
    return max_region;
}

ChartYLimits computeStudyRegionYLimits(std::span<const CStudySeries> series,
                                       int chart_region,
                                       const ChartVisibleWindow& win,
                                       int bar_count,
                                       float padding_pct,
                                       double extra_pad_frac,
                                       double move_offset) noexcept
{
    ChartYLimits out;
    out.min = 0.0;
    out.max = 1.0;
    if (bar_count <= 0)
    {
        return out;
    }
    const int first = std::clamp(win.first, 0, bar_count - 1);
    const int last = std::clamp(win.last, 0, bar_count - 1);
    if (first > last)
    {
        return out;
    }

    const int region = clampStudyChartRegion(chart_region);
    bool any = false;
    bool anchor = false;
    double lo = 0.0;
    double hi = 0.0;
    for (const CStudySeries& item : series)
    {
        if (clampStudyChartRegion(item.chart_region) != region ||
            item.values.size() != static_cast<std::size_t>(bar_count))
        {
            continue;
        }
        if (item.anchor_zero)
        {
            anchor = true;
        }
        for (int i = first; i <= last; ++i)
        {
            const double value = item.values[static_cast<std::size_t>(i)];
            if (!std::isfinite(value))
            {
                continue;
            }
            if (!any)
            {
                any = true;
                lo = value;
                hi = value;
            }
            else
            {
                lo = std::min(lo, value);
                hi = std::max(hi, value);
            }
        }
    }
    if (!any)
    {
        return out;
    }
    if (anchor)
    {
        lo = std::min(lo, 0.0);
        hi = std::max(hi, 0.0);
    }
    if (lo >= hi)
    {
        const double pad = std::abs(lo) * 0.01;
        const double used = pad > 0.0 ? pad : 1.0;
        lo -= used;
        hi += used;
    }
    const double data_range = hi - lo;
    const double pad = data_range * static_cast<double>(padding_pct) / 100.0;
    const double extra_pad = data_range * extra_pad_frac;
    out.min = lo - pad - extra_pad + move_offset;
    out.max = hi + pad + extra_pad + move_offset;
    if (out.max <= out.min)
    {
        out.max = out.min + 1.0;
    }
    return out;
}

}  // namespace terminal
