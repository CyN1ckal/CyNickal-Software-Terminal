// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "chart/CStudy.h"

#include <vector>

namespace terminal {

// List, add/remove, and the selected row's parameter widgets.
// True only when Length is submitted with Enter. Step stays 0 so +/- cannot commit:
// vendored InputScalar treats those buttons as a true return under EnterReturnsTrue.
[[nodiscard]] bool drawStudyDraftBody(std::vector<CStudyInstance>& draft,
                                      int& selected,
                                      int& next_id);

}  // namespace terminal
