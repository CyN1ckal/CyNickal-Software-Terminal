// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "chart/CStudy.h"

#include <vector>

namespace terminal {

// Bottom-bar result. length_enter is true only when Length is submitted with Enter.
// Step stays 0 so +/- cannot commit: vendored InputScalar treats those buttons
// as a true return under EnterReturnsTrue.
struct StudyDraftUi
{
    bool length_enter{false};
    bool ok{false};
    bool apply{false};
    bool cancel{false};
};

// Left list of studies on the chart, settings for the selection on the right,
// and Add Study / Remove / OK / Apply / Cancel along the bottom.
// Add Study opens a modal listing registered studies in alphabetical order.
[[nodiscard]] StudyDraftUi drawStudyDraftBody(std::vector<CStudyInstance>& draft,
                                              int& selected,
                                              int& next_id);

}  // namespace terminal
