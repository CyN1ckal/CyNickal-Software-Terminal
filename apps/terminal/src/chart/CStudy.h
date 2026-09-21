// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <variant>
#include <vector>

namespace terminal {

enum class StudyKind : std::uint8_t
{
    MovingAverage = 0
    // later: Rsi, Bollinger, Vwap — append only
};

enum class StudyPlacement : std::uint8_t
{
    Overlay = 0,  // v1 implemented: same Y as candles (price)
    Subgraph      // reserved: own plot region
};

enum class StudySource : std::uint8_t
{
    Close = 0,
    Open,
    High,
    Low
};

enum class MovingAverageMethod : std::uint8_t
{
    Simple = 0,   // v1 implemented
    Exponential,  // reserved
    Weighted      // reserved
};

inline constexpr int kStudyDefaultLength = 20;
inline constexpr int kStudyMinLength = 1;
inline constexpr int kStudyMaxLength = 10000;
inline constexpr int kStudyMaxPerPane = 16;
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

struct MovingAverageParams
{
    StudySource source{StudySource::Close};
    int length{kStudyDefaultLength};
    MovingAverageMethod method{MovingAverageMethod::Simple};
};

using StudyParams = std::variant<MovingAverageParams>;

struct CStudyInstance
{
    int id{};  // pane-local; not reused for the life of the pane
    StudyKind kind{StudyKind::MovingAverage};
    bool enabled{true};
    std::uint32_t color{kStudyDefaultColor};  // packed ImU32; no imgui.h in this header
    StudyParams params{MovingAverageParams{}};
};

struct CStudySeries
{
    int study_id{};
    StudyKind kind{StudyKind::MovingAverage};
    StudyPlacement placement{StudyPlacement::Overlay};
    std::uint32_t color{kStudyDefaultColor};
    std::string label;           // studyShortLabel; not empty after computeStudies
    std::vector<double> values;  // size == bars.size(), or 0 when bars are empty; NaN = warmup
};

struct StudyTypeInfo
{
    StudyKind kind{};
    const char* display_name{"Moving Average"};
    StudyPlacement placement{StudyPlacement::Overlay};
};

inline constexpr StudyTypeInfo kStudyTypes[] = {
    {StudyKind::MovingAverage, "Moving Average", StudyPlacement::Overlay},
};

[[nodiscard]] inline const StudyTypeInfo* findStudyType(StudyKind kind) noexcept
{
    for (const StudyTypeInfo& info : kStudyTypes)
    {
        if (info.kind == kind)
        {
            return &info;
        }
    }
    return nullptr;
}

[[nodiscard]] inline StudyParams defaultParams(StudyKind kind)
{
    switch (kind)
    {
    case StudyKind::MovingAverage:
        return MovingAverageParams{};
    }
    // No default: a missing StudyKind case must stay a -Wswitch error.
    std::abort();
}

[[nodiscard]] inline const char* studySourceCode(StudySource source) noexcept
{
    switch (source)
    {
    case StudySource::Open:
        return "O";
    case StudySource::High:
        return "H";
    case StudySource::Low:
        return "L";
    case StudySource::Close:
        return "C";
    }
    return "C";
}

}  // namespace terminal
