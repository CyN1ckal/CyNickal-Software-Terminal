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
            CStudySeries series;
            series.study_id = inst.id;
            series.kind = inst.kind;
            series.placement = info->placement;
            series.color = inst.color;
            series.label = studyShortLabel(inst);
            computeSma(bars, *params, series);
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

}  // namespace terminal
