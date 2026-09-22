// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace terminal {

// Derived from the study's chart region. Region 1 shares the price scale.
// Region 2 and above are stacked graph panes under the candles.
enum class StudyPlacement : std::uint8_t
{
    Overlay = 0,
    Subgraph
};

// Sierra Chart Chart Region. 1 is the main price graph. 2..12 are panes below
// it, and every region through the highest one in use is shown.
inline constexpr int kStudyChartRegionMin = 1;
inline constexpr int kStudyChartRegionMax = 12;
inline constexpr int kStudyMainChartRegion = 1;
inline constexpr int kStudyVolumeChartRegion = 2;
inline constexpr int kStudyMaxPerPane = 16;

[[nodiscard]] inline int clampStudyChartRegion(int region) noexcept
{
    if (region < kStudyChartRegionMin)
    {
        return kStudyChartRegionMin;
    }
    if (region > kStudyChartRegionMax)
    {
        return kStudyChartRegionMax;
    }
    return region;
}

// Packed IM_COL32 (AABBGGRR). Byte-identical to Theme::kAccent / kWarn / kOk / kDanger.
// Literals so this header does not include Theme.h or imgui.h.
inline constexpr std::uint32_t kStudyDefaultColor = 0xFFC9976Fu;
inline constexpr std::uint32_t kStudyPalette[] = {
    0xFFC9976Fu,  // Theme::kAccent  #6f97c9
    0xFF5285C0u,  // Theme::kWarn    #c08552
    0xFF638A5Fu,  // Theme::kOk      #5f8a63
    0xFF4E54B5u,  // Theme::kDanger  #b5544e
};
inline constexpr int kStudyPaletteCount = 4;

[[nodiscard]] inline std::uint32_t studyPaletteColor(int index) noexcept
{
    const int i = index % kStudyPaletteCount;
    return kStudyPalette[i < 0 ? i + kStudyPaletteCount : i];
}

[[nodiscard]] inline double studyNaN() noexcept
{
    return std::numeric_limits<double>::quiet_NaN();
}

// How a line output is stroked. Histogram outputs ignore it. Solid is the default.
enum class StudyLineStyle : std::uint8_t
{
    Solid = 0,
    Dotted,
    Dashed,
};

[[nodiscard]] inline constexpr const char* studyLineStyleToken(StudyLineStyle style) noexcept
{
    switch (style)
    {
    case StudyLineStyle::Dotted:
        return "dotted";
    case StudyLineStyle::Dashed:
        return "dashed";
    case StudyLineStyle::Solid:
        return "solid";
    }
    return "solid";
}

[[nodiscard]] inline constexpr const char* studyLineStyleLabel(StudyLineStyle style) noexcept
{
    switch (style)
    {
    case StudyLineStyle::Dotted:
        return "Dotted";
    case StudyLineStyle::Dashed:
        return "Dashed";
    case StudyLineStyle::Solid:
        return "Solid";
    }
    return "Solid";
}

[[nodiscard]] inline bool parseStudyLineStyle(std::string_view text, StudyLineStyle& style) noexcept
{
    if (text == "solid")
    {
        style = StudyLineStyle::Solid;
        return true;
    }
    if (text == "dotted")
    {
        style = StudyLineStyle::Dotted;
        return true;
    }
    if (text == "dashed")
    {
        style = StudyLineStyle::Dashed;
        return true;
    }
    return false;
}

// Color and line style for one declared output. Parallel to StudyType::outputs.
struct CStudyOutputStyle
{
    std::uint32_t color{kStudyDefaultColor};
    StudyLineStyle line{StudyLineStyle::Solid};
};

// One saved study. type_id names a registered study. options is parallel to that
// study's declared inputs. outputs is parallel to its declared traces.
// An empty outputs list means every trace uses color and a solid line.
struct CStudyInstance
{
    int id{};
    std::string type_id;
    bool enabled{true};
    std::uint32_t color{kStudyDefaultColor};
    int chart_region{kStudyMainChartRegion};
    std::vector<int> options;
    std::vector<CStudyOutputStyle> outputs;
};

// List swatch and chart-toolbar color. The first output wins once styles are set.
[[nodiscard]] inline std::uint32_t studyPrimaryColor(const CStudyInstance& inst) noexcept
{
    if (!inst.outputs.empty())
    {
        return inst.outputs.front().color;
    }
    return inst.color;
}

// One computed series. histogram, anchor_zero, and value_decimals are copied from
// the study. color and line come from the matching output.
// color_by_bar uses color for an up candle and down_color for a down candle.
struct CStudySeries
{
    int study_id{};
    std::string type_id;
    StudyPlacement placement{StudyPlacement::Overlay};
    int chart_region{kStudyMainChartRegion};
    std::uint32_t color{kStudyDefaultColor};
    std::uint32_t down_color{kStudyDefaultColor};
    StudyLineStyle line{StudyLineStyle::Solid};
    std::string label;
    std::vector<double> values;
    bool histogram{false};
    bool anchor_zero{false};
    bool color_by_bar{false};
    int value_decimals{4};
};

// Up candle uses color. Down candle uses down_color when color_by_bar is set.
[[nodiscard]] inline std::uint32_t studyHistogramColor(const CStudySeries& series, bool up) noexcept
{
    if (series.color_by_bar && !up)
    {
        return series.down_color;
    }
    return series.color;
}

}  // namespace terminal
