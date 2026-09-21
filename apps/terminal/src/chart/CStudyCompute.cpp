// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/CStudyCompute.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>

namespace terminal {
namespace {

void computeSma(std::span<const Bar> bars, const MovingAverageParams& raw, CStudySeries& out)
{
    auto params = raw;
    clampMovingAverageParams(params);
    const auto n = static_cast<int>(bars.size());
    const int length = params.length;
    out.values.assign(static_cast<std::size_t>(n), studyNaN());
    if (n <= 0 || length > n)
    {
        return;
    }
    double sum = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const auto index = static_cast<std::size_t>(i);
        sum += barSourceValue(bars[index], params.source);
        if (i >= length)
        {
            sum -= barSourceValue(bars[static_cast<std::size_t>(i - length)], params.source);
        }
        if (i >= length - 1)
        {
            out.values[index] = sum / static_cast<double>(length);
        }
    }
}

CStudySeries beginSeries(const CStudyInstance& inst)
{
    CStudySeries series;
    series.study_id = inst.id;
    series.kind = inst.kind;
    series.chart_region = clampStudyChartRegion(inst.chart_region);
    series.placement = series.chart_region == kStudyMainChartRegion ? StudyPlacement::Overlay
                                                                    : StudyPlacement::Subgraph;
    series.color = inst.color;
    series.label = studyShortLabel(inst);
    return series;
}

void computeVolume(std::span<const Bar> bars, CStudySeries& out)
{
    out.values.resize(bars.size());
    for (std::size_t i = 0; i < bars.size(); ++i)
    {
        out.values[i] = bars[i].volume;
    }
}

}  // namespace

bool isStudyInstanceSupported(const CStudyInstance& inst) noexcept
{
    if (findStudyType(inst.kind) == nullptr)
    {
        return false;
    }
    switch (inst.kind)
    {
    case StudyKind::MovingAverage:
    {
        const auto* params = std::get_if<MovingAverageParams>(&inst.params);
        if (params == nullptr)
        {
            return false;
        }
        auto clamped = *params;
        clampMovingAverageParams(clamped);
        return clamped.method == MovingAverageMethod::Simple;
    }
    case StudyKind::Volume:
        return std::holds_alternative<VolumeParams>(inst.params);
    }
    return false;
}

std::string studyShortLabel(const CStudyInstance& inst)
{
    switch (inst.kind)
    {
    case StudyKind::MovingAverage:
    {
        const auto* params = std::get_if<MovingAverageParams>(&inst.params);
        if (params == nullptr)
        {
            return {};
        }
        auto clamped = *params;
        clampMovingAverageParams(clamped);
        std::string label = "MA ";
        label += std::to_string(clamped.length);
        label += ' ';
        label += studySourceCode(clamped.source);
        return label;
    }
    case StudyKind::Volume:
        return "Vol";
    }
    return {};
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
        const auto* info = findStudyType(inst.kind);
        if (info == nullptr)
        {
            continue;
        }
        switch (inst.kind)
        {
        case StudyKind::MovingAverage:
        {
            const auto* params = std::get_if<MovingAverageParams>(&inst.params);
            if (params == nullptr)
            {
                break;
            }
            CStudySeries series = beginSeries(inst);
            computeSma(bars, *params, series);
            out.push_back(std::move(series));
            break;
        }
        case StudyKind::Volume:
        {
            if (!std::holds_alternative<VolumeParams>(inst.params))
            {
                break;
            }
            CStudySeries series = beginSeries(inst);
            computeVolume(bars, series);
            out.push_back(std::move(series));
            break;
        }
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
    bool volume = false;
    double lo = 0.0;
    double hi = 0.0;
    for (const CStudySeries& item : series)
    {
        if (clampStudyChartRegion(item.chart_region) != region ||
            item.values.size() != static_cast<std::size_t>(bar_count))
        {
            continue;
        }
        if (item.kind == StudyKind::Volume)
        {
            volume = true;
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
    if (volume)
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
