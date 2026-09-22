// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

namespace terminal {

class CChartBook;
class InventoryPanel;

// Always submitted. The bar has no close control and does not save visibility.
void drawStatusRail(const InventoryPanel& inventory, const CChartBook& charts);

}  // namespace terminal
