// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "ui/Appearance.h"

#include "chart/CChartbookFile.h"
#include "ui/Theme.h"

#include <chrono>
#include <optional>

namespace terminal::Appearance {
namespace {

using Clock = std::chrono::steady_clock;

constexpr auto kSaveDelay = std::chrono::milliseconds(400);

struct State
{
    AppearanceSettings settings{};
    bool restyle{false};
    std::optional<Clock::time_point> save_due;
    std::string error;
};

State& state()
{
    static State instance = [] {
        State loaded;
        const AppearanceLoadResult result = loadAppearanceSettings(defaultTerminalSettingsPath());
        if (result.ok)
        {
            loaded.settings = result.settings;
        }
        else
        {
            loaded.error = "appearance: " + result.error;
        }
        return loaded;
    }();
    return instance;
}

}  // namespace

const AppearanceSettings& current()
{
    return state().settings;
}

void set(const AppearanceSettings& settings)
{
    State& s = state();
    AppearanceSettings next = settings;
    next.font_px = clampFontPx(next.font_px);
    if (next == s.settings)
    {
        return;
    }
    s.settings = next;
    s.restyle = true;
    s.save_due = Clock::now() + kSaveDelay;
}

void stepFont(int delta_px)
{
    AppearanceSettings next = current();
    next.font_px = clampFontPx(next.font_px + delta_px);
    set(next);
}

void resetFont()
{
    AppearanceSettings next = current();
    next.font_px = kFontPxDefault;
    set(next);
}

void flush()
{
    State& s = state();
    if (!s.save_due.has_value())
    {
        return;
    }
    s.save_due.reset();
    const std::string error = saveAppearanceSettings(defaultTerminalSettingsPath(), s.settings);
    s.error = error.empty() ? std::string{} : "appearance: " + error;
}

bool applyPending(float dpi_scale)
{
    State& s = state();
    if (s.save_due.has_value() && Clock::now() >= *s.save_due)
    {
        flush();
    }
    if (!s.restyle)
    {
        return false;
    }
    s.restyle = false;
    Theme::apply(s.settings, dpi_scale);
    return true;
}

void applyNow(float dpi_scale)
{
    State& s = state();
    s.restyle = false;
    Theme::apply(s.settings, dpi_scale);
}

const std::string& lastError()
{
    return state().error;
}

}  // namespace terminal::Appearance
